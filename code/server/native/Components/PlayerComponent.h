#pragma once

#include "AppearanceComponent.h"

struct PlayerComponent
{
    ConnectionId Connection;
    flecs::entity Puppet;
    std::string Username;
    // Jeton de session du launcher : jamais journalisé ni déclaré à flecs (explorateur),
    // vidé dès que le plugin le lit (IPlayer::TakeSessionToken).
    std::string SessionToken;
    // Validé par un plugin (IPlayer::Authorize). Tant que c'est faux et que la config l'exige
    // (RequireScriptAuthorization), le joueur ne peut rien faire ni rien recevoir du monde.
    bool Authorized{false};
    // Apparence choisie par un plugin (IPlayer::SetAppearance) : remplace celle envoyée par le client à
    // l'apparition. Jamais déclarée à flecs (explorateur).
    bool HasAppearanceOverride{false};
    AppearanceComponent AppearanceOverride{};

    const char* GetUsername() const;

    static void Register(flecs::world& aWorld);
};
