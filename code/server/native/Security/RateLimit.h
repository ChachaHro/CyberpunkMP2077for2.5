#pragma once

// Limites de débit des messages d'un client, en fonctions pures (testées dans code/server/tests).
// Un seau de jetons par type de message : le seau se remplit à Rate jetons/s jusqu'à Burst ; chaque
// message en prend un. Seau vide : message ignoré et compté ; trop de messages ignorés : expulsion.

#include <algorithm>
#include <array>
#include <cstdint>

namespace RateLimit
{
// Codes de type des messages client (ordre du « oneof » de protocol/client.proto, à partir de 0).
enum class MessageKind : uint8_t
{
    Authentication = 0,
    SpawnCharacter = 1,
    MoveEntity = 2,
    Chat = 3,
    Rpc = 4,
    EnterVehicle = 5,
    ExitVehicle = 6,
    Count
};

struct BucketLimits
{
    float Rate{0.f};  // jetons par seconde
    float Burst{0.f}; // jetons au plus
};

// Valeurs larges : un client normal en est très loin (déplacements : personnage + véhicule à chaque mise
// à jour ; chat : 5 messages d'affilée puis un toutes les 0,8 s, comme le plugin du serveur).
inline constexpr std::array<BucketLimits, static_cast<size_t>(MessageKind::Count)> kDefaultLimits{{
    {0.2f, 2.f},    // Authentication
    {0.2f, 2.f},    // SpawnCharacter
    {60.f, 120.f},  // MoveEntity
    {1.25f, 5.f},   // Chat
    {20.f, 40.f},   // Rpc
    {2.f, 6.f},     // EnterVehicle
    {2.f, 6.f},     // ExitVehicle
}};

struct Bucket
{
    float Tokens{0.f};
    bool Started{false};
};

// Vrai si le message passe. aScale multiplie débit et réserve (0 ou moins = aucune limite).
inline bool TryConsume(Bucket& aBucket, const BucketLimits& aLimits, float aElapsedSeconds, float aScale = 1.f) noexcept
{
    if (!(aScale > 0.f))
        return true;

    const float rate = aLimits.Rate * aScale;
    const float burst = aLimits.Burst * aScale;
    if (!aBucket.Started)
    {
        aBucket.Tokens = burst;
        aBucket.Started = true;
    }
    else if (aElapsedSeconds > 0.f)
    {
        aBucket.Tokens = (std::min)(burst, aBucket.Tokens + rate * aElapsedSeconds);
    }

    if (aBucket.Tokens >= 1.f)
    {
        aBucket.Tokens -= 1.f;
        return true;
    }
    return false;
}

// Messages ignorés d'une connexion : s'effacent à aDecayPerSecond ; expulsion à aKickAt (0 = jamais).
struct Offences
{
    float Count{0.f};
};

inline bool RecordDrop(Offences& aOffences, float aElapsedSeconds, float aDecayPerSecond, uint32_t aKickAt) noexcept
{
    if (aElapsedSeconds > 0.f && aDecayPerSecond > 0.f)
        aOffences.Count = (std::max)(0.f, aOffences.Count - aDecayPerSecond * aElapsedSeconds);
    aOffences.Count += 1.f;
    return aKickAt != 0 && aOffences.Count >= static_cast<float>(aKickAt);
}

inline bool IsKnownKind(uint64_t aOpcode) noexcept
{
    return aOpcode < static_cast<uint64_t>(MessageKind::Count);
}
} // namespace RateLimit
