#pragma once

#include <chrono>

#include "Security/GameplayRules.h"

// État du contrôle de vitesse d'une entité (personnage ou véhicule), côté serveur seulement : jamais
// répliqué ni exposé aux scripts.
struct MovementGuardComponent
{
    GameplayRules::MovementState State{};
    std::chrono::steady_clock::time_point LastMove{};
    bool Started{false};
};
