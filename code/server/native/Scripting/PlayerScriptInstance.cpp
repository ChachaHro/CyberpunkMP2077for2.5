#include "PlayerScriptInstance.h"
#include "Components/PlayerComponent.h"
#include <GameServer.h>
#include <PlayerManager.h>
#include "IRpc.h"
#include "Components/AppearanceComponent.h"
#include "Game/Level.h"
#include "Security/AppearanceRules.h"


PlayerScriptInstance::PlayerScriptInstance(flecs::entity aPlayer)
    : m_player(aPlayer)
    , m_movementComponent(aPlayer)
    , m_attachmentComponent(aPlayer)
{
}

PlayerScriptInstance::~PlayerScriptInstance()
{
}

uint64_t PlayerScriptInstance::GetId() const
{
    return m_player.raw_id();
}

uint32_t PlayerScriptInstance::GetConnectionId() const
{
    if (const auto pComponent = m_player.get<PlayerComponent>())
        return pComponent->Connection;

    return -1;
}

uint64_t PlayerScriptInstance::GetPuppetId() const
{
    if (const auto pComponent = m_player.get<PlayerComponent>())
        return pComponent->Puppet.raw_id();

    return -1;
}

IMovementComponent* PlayerScriptInstance::GetMovementComponent()
{
    return &m_movementComponent;
}

IAttachmentComponent* PlayerScriptInstance::GetAttachmentComponent()
{
    return &m_attachmentComponent;
}

const char* PlayerScriptInstance::GetUsername() const
{
    if (const auto pComponent = m_player.get<PlayerComponent>())
        return pComponent->Username.c_str();

    return "None";
}

void PlayerScriptInstance::SendChat(const char* From, const char* Message)
{
    if (const auto pComponent = m_player.get<PlayerComponent>())
    {
        server::ChatMessage msg;
        msg.set_username(From);
        msg.set_message(Message);
        GServer->Send(pComponent->Connection, msg);
    }
    else
        spdlog::get("Scripting")->warn("Attempt to send a message to a player that doesn't have a PlayerComponent!");
}

void PlayerScriptInstance::Call(uint64_t Klass, uint64_t Function, IBuffer* Arguments)
{
    IRpc::Get()->Call(GetId(), Klass, Function, Arguments);
}

const char* PlayerScriptInstance::TakeSessionToken()
{
    // Copié par le marshalling .NET dès le retour : le jeton ne reste nulle part en mémoire native.
    thread_local std::string s_token;
    s_token.clear();
    if (auto* pComponent = m_player.get_mut<PlayerComponent>())
        std::swap(s_token, pComponent->SessionToken);
    return s_token.c_str();
}

void PlayerScriptInstance::Kick()
{
    const auto pComponent = m_player.get<PlayerComponent>();
    if (!pComponent)
        return;

    // Server::Kick détruit le joueur immédiatement : on le diffère pour ne pas invalider l'appelant
    // (le plugin est souvent encore dans son callback d'arrivée du joueur).
    const auto connection = pComponent->Connection;
    GServer->GetTaskQueue()->Add([connection]
    {
        if (GServer->GetWorld()->get_mut<PlayerManager>()->GetByConnectionId(connection).is_valid())
            GServer->Kick(connection);
    });
}

void PlayerScriptInstance::Authorize()
{
    // get_mut sans modified() : ne redéclenche pas l'observer d'arrivée du joueur.
    if (auto* pComponent = m_player.get_mut<PlayerComponent>())
        pComponent->Authorized = true;
}

bool PlayerScriptInstance::SetAppearance(const char* Appearance)
{
    if (!Appearance)
        return false;

    const std::string_view text(Appearance);
    if (text.empty() || text.size() > AppearanceRules::kMaxTextBytes)
        return false;

    AppearanceComponent appearance;
    try
    {
        const auto data = nlohmann::json::parse(text);
        if (!data.is_object() || !data.contains("v") || data["v"] != 1)
            return false;

        std::vector<uint8_t> bytes;
        if (!AppearanceRules::DecodeHex(data.at("ccstate").get<std::string>(), bytes) || bytes.empty())
            return false;
        appearance.ccstate.assign(bytes.begin(), bytes.end());

        const auto& items = data.at("equipment");
        if (!items.is_array() || items.size() > AppearanceRules::kMaxItems)
            return false;
        for (const auto& item : items)
        {
            const auto name = item.get<std::string>();
            if (!AppearanceRules::IsValidItemName(name))
                return false;
            appearance.equipment.push_back(name.c_str());
        }
    }
    catch (const std::exception&)
    {
        return false;
    }

    // get_mut sans modified() : ne redéclenche pas l'observer d'arrivée du joueur.
    auto* pComponent = m_player.get_mut<PlayerComponent>();
    if (!pComponent)
        return false;

    pComponent->AppearanceOverride = appearance;
    pComponent->HasAppearanceOverride = true;

    if (pComponent->Puppet && pComponent->Puppet.is_alive())
    {
        pComponent->Puppet.set<AppearanceComponent>(appearance);
        GServer->GetWorld()->get_mut<Level>()->RefreshAppearance(pComponent->Puppet);
    }

    spdlog::info("Appearance set for player {:x}: {} bytes, {} item(s)", m_player.id(), appearance.ccstate.size(),
                 appearance.equipment.size());
    return true;
}
