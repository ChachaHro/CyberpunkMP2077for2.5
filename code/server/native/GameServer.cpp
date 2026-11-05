#include "GameServer.h"

#include <Components/PlayerComponent.h>
#include "Security/ConnectionRules.h"

#include "Core/Filesystem.h"
#include "Game/Level.h"
#include "Scripting/IRpc.h"
#include "Scripting/RpcScriptInstance.h"

#include "PlayerManager.h"


using nlohmann::json;

GameServer* GServer = nullptr;

GameServer::GameServer()
    : Server(client::kIdentifier, server::kIdentifier)
    , m_log(m_path)
    , m_lastUpdate(std::chrono::steady_clock::now())
{
    GServer = this;

    try
    {
        auto serverPath = GetPath();

        std::ifstream config(serverPath / "config" / "server.json");
        if (!config.is_open())
        {
            spdlog::info("No configuration file found, creating config/server.json");
            std::error_code ec;
            create_directory(serverPath / "config", ec);

            std::ofstream of(serverPath / "config" / "server.json");
            json data = m_config;
            of << std::setw(4) << data;
        }
        else
        {
            json data = json::parse(config);
            m_config = data.get<Config>();
        }
    }
    catch (std::exception& e)
    {
        spdlog::error("Error parsing config.json: {}", e.what());
        m_run = false;
        return;
    }

    uint16_t port = m_config.Port;
    while (!Host(port, m_config.TickRate))
    {
        spdlog::warn("Port {} is already in use, trying {}", port, port + 1);
        port++;
    }

    m_pWorld = MakeUnique<World>(m_config.GetFlecsConfig());
    m_pWorld->GetScriptInstance()->Initialize();

    RegisterHandler<&GameServer::HandleAuthentication>(this);

    spdlog::info("Server started on port {}", GetPort());
}

GameServer::~GameServer()
{
    GServer = nullptr;
}

void GameServer::Kill()
{
    m_run = false;
    Close();
}

void GameServer::Run()
{
    while (m_run && IsListening())
        Update();
}

void GameServer::OnUpdate()
{
    const auto now = std::chrono::steady_clock::now();
    const auto delta = now - m_lastUpdate;
    m_lastUpdate = now;

    m_tasks.Drain();
    m_dispatcher.update();

    m_pWorld->Update(std::chrono::duration_cast<std::chrono::duration<float>>(delta).count());
}

void GameServer::OnConsume(const void* apData, uint32_t aSize, ConnectionId aConnectionId)
{
    // ReSharper disable once CppCStyleCast
    ViewBuffer buffer((uint8_t*)apData, aSize);  // NOLINT(clang-diagnostic-cast-qual)

    // Type du message (premier entier du paquet) lu à part, pour la limite de débit avant décodage
    {
        Buffer::Reader peek(&buffer);
        const uint64_t opcode = Serialization::ReadVarInt(peek);
        bool kick = false;
        if (!AllowMessage(aConnectionId, opcode, kick))
        {
            if (kick)
            {
                spdlog::warn("Connection {:x} kicked: too many messages", aConnectionId);
                Kick(aConnectionId);
            }
            return;
        }
    }

    Buffer::Reader reader(&buffer);

    if (const auto result = client::Deserializer::Process(reader, aConnectionId, m_dispatcher); !result)
    {
        spdlog::warn("Failed to deserialize packet from connection {:x}, kicking", aConnectionId);
        Kick(aConnectionId);
    }
}

bool GameServer::AllowMessage(ConnectionId aConnectionId, uint64_t aOpcode, bool& aKick) noexcept
{
    aKick = false;

    if (!RateLimit::IsKnownKind(aOpcode))
    {
        spdlog::warn("Unknown message type {} from connection {:x}, kicking", aOpcode, aConnectionId);
        aKick = true;
        return false;
    }

    if (!(m_config.RateLimitScale > 0.f))
        return true;

    const auto now = std::chrono::steady_clock::now();
    auto& state = m_rateLimits[aConnectionId];
    const auto index = static_cast<size_t>(aOpcode);

    auto limits = RateLimit::kDefaultLimits[index];
    // Les déplacements suivent UpdateRate (personnage + véhicule à chaque mise à jour) : jamais moins de 4 par mise à jour
    if (static_cast<RateLimit::MessageKind>(index) == RateLimit::MessageKind::MoveEntity)
    {
        limits.Rate = (std::max)(limits.Rate, 4.f * m_config.UpdateRate);
        limits.Burst = (std::max)(limits.Burst, 2.f * limits.Rate);
    }

    const float elapsed = std::chrono::duration<float>(now - state.LastMessage[index]).count();
    state.LastMessage[index] = now;

    if (RateLimit::TryConsume(state.Buckets[index], limits, elapsed, m_config.RateLimitScale))
        return true;

    const float sinceDrop = std::chrono::duration<float>(now - state.LastDrop).count();
    state.LastDrop = now;
    aKick = RateLimit::RecordDrop(state.Offences, sinceDrop, 10.f, m_config.RateLimitKickDrops);
    if (!aKick && state.Offences.Count <= 1.f)
        spdlog::warn("Connection {:x} sends too many messages of type {}: ignoring the excess", aConnectionId, aOpcode);
    return false;
}

void GameServer::OnConnection(ConnectionId aHandle)
{
    // Log at info, with the peer address. At debug level this is invisible by
    // default, which makes "the client cannot connect" impossible to diagnose:
    // there is no way to tell a client that never reached us from one that
    // reached us and failed later.
    char address[SteamNetworkingIPAddr::k_cchMaxString] = {};
    const auto info = GetConnectionInfo(aHandle);
    info.m_addrRemote.ToString(address, sizeof(address), true);

    spdlog::info("Connection received from {} (id {:x})", address, aHandle);
}

void GameServer::OnDisconnection(ConnectionId aConnectionId, EDisconnectReason aReason)
{
    spdlog::info("Connection {:x} ended (reason {})", aConnectionId, static_cast<uint32_t>(aReason));

    m_rateLimits.erase(aConnectionId);

    auto* pPlayerManager = GetWorld()->get_mut<PlayerManager>();

    if (const auto player = pPlayerManager->GetByConnectionId(aConnectionId))
    {
        pPlayerManager->Remove(player);

        // Véhicules d'abord : détruits avec le joueur, ils resteraient en fantômes chez les autres clients
        GetWorld()->get_mut<Level>()->RemovePlayerVehicles(player);

        if (auto* pPlayerComponent = player.get<PlayerComponent>())
        {
            GetWorld()->get_mut<Level>()->Remove(pPlayerComponent->Puppet);
        }

        player.destruct();

        spdlog::info("{}/{} player(s) online", pPlayerManager->Count(), m_config.MaxPlayer);
    }
}

void GameServer::HandleAuthentication(const PacketEvent<client::AuthenticationRequest>& aRequest)
{
    server::AuthenticationResponse response;

    if (aRequest.get_client_protocol() != client::kIdentifier)
    {
        response.set_success(false);
        response.set_error("Invalid protocol version!");

        spdlog::warn("Connection attempt with client identifier {:x}, expected {:x}", aRequest.get_client_protocol(), client::kIdentifier);
        Send(aRequest.ConnectionId, response);
        Kick(aRequest.ConnectionId);
        return;
    }

    if (aRequest.get_server_protocol() != server::kIdentifier)
    {
        response.set_success(false);
        response.set_error("Invalid protocol version!");

        spdlog::warn("Connection attempt with server identifier {:x}, expected {:x}", aRequest.get_client_protocol(), server::kIdentifier);
        Send(aRequest.ConnectionId, response);
        Kick(aRequest.ConnectionId);
        return;
    }

    auto* pPlayerManager = GetWorld()->get_mut<PlayerManager>();

    // Une seule authentification par connexion : une seconde demande est ignorée.
    if (pPlayerManager->GetByConnectionId(aRequest.ConnectionId))
    {
        spdlog::warn("Connection {:x} sent a second authentication request, ignored", aRequest.ConnectionId);
        return;
    }

    const auto refuse = [&](const char* acError, const char* acReason)
    {
        response.set_success(false);
        response.set_error(acError);

        spdlog::warn("Connection {:x} refused ({}): username \"{}\"", aRequest.ConnectionId, acReason,
                     ConnectionRules::SanitizeForLog(aRequest.get_username(), 64));
        Send(aRequest.ConnectionId, response);
        Kick(aRequest.ConnectionId);
    };

    // Jeton de session du launcher : validé par le plugin du serveur à l'arrivée du joueur. Jamais journalisé.
    constexpr size_t kMaxSessionTokenLength = 256;
    const auto& token = aRequest.get_token();
    if (token.size() > kMaxSessionTokenLength)
    {
        refuse("Invalid session!", "oversized session token");
        return;
    }

    const auto& username = aRequest.get_username();
    if (!ConnectionRules::IsValidUsername(username))
    {
        refuse("Invalid username!", "invalid username");
        return;
    }

    if (ConnectionRules::IsServerFull(pPlayerManager->Count(), m_config.MaxPlayer))
    {
        refuse("Server is full!", "server full");
        return;
    }

    bool duplicate = false;
    pPlayerManager->ForEach([&](flecs::entity aPlayer) {
        if (const auto* pComponent = aPlayer.get<PlayerComponent>())
            duplicate = duplicate || ConnectionRules::SameUsername(pComponent->Username, username);
    });
    if (duplicate)
    {
        refuse("This username is already connected!", "username already connected");
        return;
    }

    spdlog::info("Authorize connection from {}", ConnectionRules::SanitizeForLog(username, 64));
    response.set_success(true);

    server::Settings settings;
    settings.set_update_rate(m_config.UpdateRate);
    response.set_settings(settings);

    server::RpcDefinitions definitions;

    const auto* pRpc = static_cast<RpcScriptInstance*>(IRpc::Get());
    pRpc->Serialize(definitions);

    if (!Send(aRequest.ConnectionId, definitions))
        spdlog::error("Failed to send message to {:x}", aRequest.ConnectionId);

    if (!Send(aRequest.ConnectionId, response))
        spdlog::error("Failed to send message to {:x}", aRequest.ConnectionId);

    // The player was accepted, rpc definitions are ready, we can create the player's handle
    pPlayerManager->Create(aRequest.ConnectionId, username, String(token.c_str()));
}

bool GameServer::IsPlayerAllowed(const PlayerComponent& aPlayer) const noexcept
{
    return !m_config.RequireScriptAuthorization || aPlayer.Authorized;
}

ScratchAllocator& GameServer::GetScratch()
{
    thread_local ScratchAllocator s_allocator{1 << 19};
    return s_allocator;
}
