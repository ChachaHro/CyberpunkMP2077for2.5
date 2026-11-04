#pragma once

struct PlayerComponent
{
    ConnectionId Connection;
    flecs::entity Puppet;
    std::string Username;
    // Jeton de session du launcher : jamais journalisé ni déclaré à flecs (explorateur),
    // vidé dès que le plugin le lit (IPlayer::TakeSessionToken).
    std::string SessionToken;

    const char* GetUsername() const;

    static void Register(flecs::world& aWorld);
};
