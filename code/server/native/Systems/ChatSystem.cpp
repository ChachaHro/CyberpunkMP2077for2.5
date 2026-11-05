#include "ChatSystem.h"

#include "GameServer.h"
#include "Components/PlayerComponent.h"

#include "PlayerManager.h"
#include "Security/ConnectionRules.h"

ChatSystem::ChatSystem(gsl::not_null<World*> apWorld)
    : m_pWorld(apWorld)
{
    GServer->RegisterHandler<&ChatSystem::HandleChatMessageRequest>(this);
}

void ChatSystem::Broadcast(String acUsername, String acMessage)
{
    server::ChatMessage message;
    message.set_username(std::move(acUsername));
    message.set_message(std::move(acMessage));

    m_pWorld->each([&message](flecs::entity, const PlayerComponent& aPlayer)
    {
        if (!GServer->IsPlayerAllowed(aPlayer))
            return;

        GServer->Send(aPlayer.Connection, message);
    });
}

void ChatSystem::HandleChatMessageRequest(const PacketEvent<client::ChatMessageRequest>& aMessage)
{
    auto* pPlayerManager = m_pWorld->get<PlayerManager>();

    const auto entity = pPlayerManager->GetByConnectionId(aMessage.ConnectionId);
    if (!entity || !entity.has<PlayerComponent>())
    {
        spdlog::error("Received chat message from connection with no associated player!");
        return;
    }
    auto* pPlayer = entity.get<PlayerComponent>();
    if (!GServer->IsPlayerAllowed(*pPlayer))
        return;

    // Texte du client nettoyé avant journalisation : pas de fausses lignes (retours à la ligne, ANSI).
    spdlog::info("[chat] [{}]: {}", ConnectionRules::SanitizeForLog(pPlayer->Username, 64),
                 ConnectionRules::SanitizeForLog(aMessage.get_message()));

    Broadcast(pPlayer->Username.c_str(), aMessage.get_message());
}
