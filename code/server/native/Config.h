#pragma once

#include "Scripting/IConfig.h"

struct FlecsConfig
{
    bool Enabled{true};
    uint16_t Port{27750};
    std::string IpAddress{"127.0.0.1"};

    bool IsEnabled() const { return Enabled; }
    uint16_t GetPort() const { return Port; }
    const char* GetIpAddress() const { return IpAddress.c_str(); }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FlecsConfig, Enabled, Port, IpAddress)
};

struct Config : IConfig
{
    std::string Name{"Default"};
    std::string Description{};
    std::string ApiKey{};
    std::string IconUrl{};
    uint16_t MaxPlayer{4};
    std::string Tags{};
    bool Public{false};
    uint16_t Port{11778};
    uint16_t WebPort{11779};
    uint16_t TickRate{60};
    uint16_t UpdateRate{10};
    std::string Password{};
    // Si vrai, un joueur ne peut rien faire (apparaître, bouger, véhicules, chat, RPC) et ne reçoit pas
    // le monde tant qu'un plugin ne l'a pas validé (IPlayer::Authorize). Faux par défaut (compatibilité).
    bool RequireScriptAuthorization{false};
    // Véhicules. Distance maximale (mètres) entre un personnage et le véhicule distant où il monte
    // (0 = aucune limite). Véhicules que le serveur garde par joueur : au-delà, le plus ancien est retiré
    // (0 = aucune limite). Prendre le volant du véhicule d'un autre joueur connecté : refusé par défaut.
    float MaxVehicleEnterDistance{10.f};
    uint16_t MaxVehiclesPerPlayer{1};
    bool AllowVehicleTakeover{false};
    // Déplacements. Vitesses maximales (m/s ; 0 = aucune limite) : à pied (horizontale, pour ne pas
    // compter les chutes) et en véhicule. Larges exprès : métro, voyage rapide et ascenseurs restent
    // permis, seule la triche grossière est visée. Un dépassement est une infraction ; une infraction
    // s'efface toutes les MovementStrikeDecaySeconds ; expulsion à MovementKickStrikes (0 = journal seulement).
    float MaxFootSpeed{50.f};
    float MaxVehicleSpeed{150.f};
    float MovementBurstSeconds{3.f};
    float MovementStrikeDecaySeconds{60.f};
    uint16_t MovementKickStrikes{10};
    // Débit des messages client (Security/RateLimit.h). RateLimitScale multiplie toutes les limites
    // (0 = aucune limite). Un message au-delà est ignoré ; expulsion à RateLimitKickDrops messages ignorés
    // (le compteur baisse de 10 par seconde ; 0 = jamais d'expulsion).
    float RateLimitScale{1.f};
    uint16_t RateLimitKickDrops{100};
    FlecsConfig Flecs{};

    const char* GetName() const override { return Name.c_str(); }
    const char* GetDescription() const override { return Description.c_str(); }
    const char* GetApiKey() const override { return ApiKey.c_str(); }
    const char* GetIconUrl() const override { return IconUrl.c_str(); }
    uint16_t GetMaxPlayer() const override { return MaxPlayer; }
    const char* GetTags() const override { return Tags.c_str(); }
    bool GetPublic() const override { return Public; }
    uint16_t GetPort() const override { return Port; }
    uint16_t GetWebPort() const override { return WebPort; }
    uint16_t GetTickRate() const override { return TickRate; }
    uint16_t GetUpdateRate() const override { return UpdateRate; }
    const char* GetPassword() const { return Password.c_str(); }
    const FlecsConfig& GetFlecsConfig() const { return Flecs; }

    // WITH_DEFAULT : une clé absente du fichier garde sa valeur par défaut au lieu d'empêcher le démarrage.
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, Name, Description, IconUrl, MaxPlayer, Tags, TickRate, UpdateRate, Public, Port,
                                                Password, ApiKey, RequireScriptAuthorization, MaxVehicleEnterDistance,
                                                MaxVehiclesPerPlayer, AllowVehicleTakeover, MaxFootSpeed, MaxVehicleSpeed,
                                                MovementBurstSeconds, MovementStrikeDecaySeconds, MovementKickStrikes, RateLimitScale,
                                                RateLimitKickDrops, Flecs)
};