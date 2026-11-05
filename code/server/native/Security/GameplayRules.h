#pragma once

// Règles de jeu côté serveur, en fonctions pures (aucune dépendance au moteur) pour être testées
// seules : code/server/tests. Les positions sont en mètres (position du monde du jeu).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace GameplayRules
{
// Identifiant (hash de CName) du siège conducteur « seat_front_left » : l'occuper donne le contrôle.
inline constexpr uint64_t kDriverSeatId = 0xb000b1d029d0cea0ULL;

struct Vec3
{
    float x{0.f};
    float y{0.f};
    float z{0.f};
};

inline float DistanceSquared(const Vec3& aLeft, const Vec3& aRight) noexcept
{
    const float dx = aLeft.x - aRight.x;
    const float dy = aLeft.y - aRight.y;
    const float dz = aLeft.z - aRight.z;
    return dx * dx + dy * dy + dz * dz;
}

// Bornes de la carte, très larges (la ville tient dans ±10 km) : au-delà, ou NaN / infini, la position
// est forgée et ne doit être ni gardée ni renvoyée aux autres clients.
inline constexpr float kMaxWorldCoordinate = 100000.f;

inline bool IsSaneWorldPosition(const Vec3& aPosition) noexcept
{
    const auto ok = [](float aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kMaxWorldCoordinate; };
    return ok(aPosition.x) && ok(aPosition.y) && ok(aPosition.z);
}

// aMaxDistance <= 0 signifie « aucune limite ».
inline bool IsWithinDistance(const Vec3& aLeft, const Vec3& aRight, float aMaxDistance) noexcept
{
    if (!(aMaxDistance > 0.f))
        return true;
    return DistanceSquared(aLeft, aRight) <= aMaxDistance * aMaxDistance;
}

// Prendre le siège conducteur d'un véhicule appartenant à un autre joueur connecté : refusé sauf si la
// config l'autorise. Le véhicule d'un joueur reste le sien tant qu'il est connecté.
inline bool CanTakeDriverSeat(bool aVehicleOwnedByOtherConnectedPlayer, bool aAllowTakeover) noexcept
{
    return aAllowTakeover || !aVehicleOwnedByOtherConnectedPlayer;
}

inline float HorizontalDistance(const Vec3& aLeft, const Vec3& aRight) noexcept
{
    const float dx = aLeft.x - aRight.x;
    const float dy = aLeft.y - aRight.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline float Distance(const Vec3& aLeft, const Vec3& aRight) noexcept
{
    return std::sqrt(DistanceSquared(aLeft, aRight));
}

// Déplacements. Le serveur ne peut pas corriger la position d'un client (pas de message pour ça) : un
// déplacement trop rapide est donc accepté mais compté comme infraction. Les infractions s'effacent avec
// le temps (voyage rapide, ascenseur : une seule) ; une triche continue les accumule et mène à l'expulsion.
struct MovementLimits
{
    float MaxSpeed{0.f};           // m/s ; 0 = aucune limite
    float BurstSeconds{3.f};       // réserve de distance : MaxSpeed × BurstSeconds au plus
    float StrikeDecaySeconds{60.f}; // une infraction effacée toutes les N secondes (0 = jamais)
};

struct MovementState
{
    float Budget{0.f};  // distance encore autorisée (m)
    float Strikes{0.f}; // infractions en cours
};

inline MovementState FreshMovementState(const MovementLimits& aLimits) noexcept
{
    return {aLimits.MaxSpeed * aLimits.BurstSeconds, 0.f};
}

// Consomme aDistance (m) après aElapsedSeconds depuis le déplacement précédent. Vrai si c'est dans la
// limite ; faux = infraction (la réserve repart de zéro pour ne pas punir les déplacements suivants).
inline bool ConsumeMovement(MovementState& aState, float aDistance, float aElapsedSeconds, const MovementLimits& aLimits) noexcept
{
    if (!(aElapsedSeconds > 0.f))
        aElapsedSeconds = 0.f;

    if (aLimits.StrikeDecaySeconds > 0.f)
        aState.Strikes = (std::max)(0.f, aState.Strikes - aElapsedSeconds / aLimits.StrikeDecaySeconds);

    if (!(aLimits.MaxSpeed > 0.f))
        return true;

    const float cap = aLimits.MaxSpeed * (std::max)(aLimits.BurstSeconds, 0.f);
    aState.Budget = (std::min)(cap, aState.Budget + aLimits.MaxSpeed * aElapsedSeconds);
    // Après un long silence (le client n'envoie rien pendant certains trajets en véhicule), le joueur a pu
    // parcourir jusqu'à MaxSpeed × durée : se taire ne fait pas gagner de vitesse moyenne.
    const float allowance = (std::max)(aState.Budget, aLimits.MaxSpeed * aElapsedSeconds);

    // NaN ou infini : toujours une infraction
    if (aDistance >= 0.f && aDistance <= allowance)
    {
        aState.Budget = (std::min)(cap, allowance - aDistance);
        return true;
    }

    aState.Budget = 0.f;
    aState.Strikes += 1.f;
    return false;
}

// aKickStrikes = 0 : jamais d'expulsion (journal seulement).
inline bool ShouldKickForMovement(const MovementState& aState, uint32_t aKickStrikes) noexcept
{
    return aKickStrikes != 0 && aState.Strikes >= static_cast<float>(aKickStrikes);
}

// Combien de véhicules déjà possédés faut-il retirer avant d'en faire apparaître un nouveau ?
// aMaxPerPlayer = 0 signifie « aucune limite ».
inline uint32_t VehiclesToRemoveBeforeSpawn(uint32_t aOwned, uint32_t aMaxPerPlayer) noexcept
{
    if (aMaxPerPlayer == 0 || aOwned < aMaxPerPlayer)
        return 0;
    return aOwned - aMaxPerPlayer + 1;
}
} // namespace GameplayRules
