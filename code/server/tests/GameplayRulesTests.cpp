#include <catch2/catch.hpp>

#include "Security/GameplayRules.h"

using namespace GameplayRules;

TEST_CASE("Distance : à côté du véhicule, accepté")
{
    CHECK(IsWithinDistance({0.f, 0.f, 0.f}, {3.f, 4.f, 0.f}, 10.f));   // 5 m
    CHECK(IsWithinDistance({0.f, 0.f, 0.f}, {6.f, 8.f, 0.f}, 10.f));   // 10 m pile
    CHECK(IsWithinDistance({100.f, -50.f, 2.f}, {100.f, -50.f, 2.f}, 10.f));
}

TEST_CASE("Distance : trop loin, refusé")
{
    CHECK_FALSE(IsWithinDistance({0.f, 0.f, 0.f}, {6.f, 8.1f, 0.f}, 10.f));
    CHECK_FALSE(IsWithinDistance({0.f, 0.f, 0.f}, {0.f, 0.f, 50.f}, 10.f)); // en hauteur aussi
    CHECK_FALSE(IsWithinDistance({-1500.f, 300.f, 0.f}, {2000.f, -800.f, 0.f}, 10.f));
}

TEST_CASE("Distance : 0 ou négatif = aucune limite")
{
    CHECK(IsWithinDistance({0.f, 0.f, 0.f}, {5000.f, 0.f, 0.f}, 0.f));
    CHECK(IsWithinDistance({0.f, 0.f, 0.f}, {5000.f, 0.f, 0.f}, -1.f));
}

TEST_CASE("Volant : jamais celui d'un autre joueur connecté, sauf config")
{
    CHECK(CanTakeDriverSeat(false, false));
    CHECK_FALSE(CanTakeDriverSeat(true, false));
    CHECK(CanTakeDriverSeat(true, true));
}

TEST_CASE("Véhicules par joueur : le plus ancien part au-delà de la limite")
{
    CHECK(VehiclesToRemoveBeforeSpawn(0, 1) == 0);
    CHECK(VehiclesToRemoveBeforeSpawn(1, 1) == 1);
    CHECK(VehiclesToRemoveBeforeSpawn(3, 1) == 3);
    CHECK(VehiclesToRemoveBeforeSpawn(1, 2) == 0);
    CHECK(VehiclesToRemoveBeforeSpawn(2, 2) == 1);
    CHECK(VehiclesToRemoveBeforeSpawn(5, 0) == 0); // 0 = aucune limite
}

TEST_CASE("Positions forgées refusées")
{
    CHECK(IsSaneWorldPosition({-1500.f, 300.f, 20.f}));
    CHECK_FALSE(IsSaneWorldPosition({std::nanf(""), 0.f, 0.f}));
    CHECK_FALSE(IsSaneWorldPosition({0.f, INFINITY, 0.f}));
    CHECK_FALSE(IsSaneWorldPosition({0.f, 0.f, 1e9f}));
}

namespace
{
constexpr MovementLimits kFoot{50.f, 3.f, 60.f};

// Marche régulière : aSpeed m/s, un message tous les 0,1 s pendant aSeconds
uint32_t Walk(MovementState& aState, float aSpeed, float aSeconds, const MovementLimits& aLimits = kFoot)
{
    uint32_t refused = 0;
    for (float t = 0.f; t < aSeconds; t += 0.1f)
        if (!ConsumeMovement(aState, aSpeed * 0.1f, 0.1f, aLimits))
            ++refused;
    return refused;
}
} // namespace

TEST_CASE("Mouvements : marche, course et métro restent permis")
{
    auto state = FreshMovementState(kFoot);
    CHECK(Walk(state, 7.f, 60.f) == 0);   // course
    CHECK(Walk(state, 30.f, 120.f) == 0); // métro NCART
    CHECK(state.Strikes == 0.f);
}

TEST_CASE("Mouvements : paquets groupés après un ralentissement réseau")
{
    auto state = FreshMovementState(kFoot);
    // 1 s sans message puis 10 messages d'un coup (7 m/s) : la réserve absorbe
    CHECK(ConsumeMovement(state, 0.7f, 1.0f, kFoot));
    for (int i = 0; i < 9; ++i)
        CHECK(ConsumeMovement(state, 0.7f, 0.f, kFoot));
}

TEST_CASE("Mouvements : un voyage rapide = une seule infraction, effacée ensuite")
{
    auto state = FreshMovementState(kFoot);
    CHECK_FALSE(ConsumeMovement(state, 3000.f, 0.1f, kFoot));
    CHECK(state.Strikes == 1.f);
    CHECK_FALSE(ShouldKickForMovement(state, 10));
    CHECK(Walk(state, 7.f, 61.f) == 0); // on repart normalement, et l'infraction s'efface
    CHECK(state.Strikes == 0.f);
}

TEST_CASE("Mouvements : une triche continue mène à l'expulsion")
{
    auto state = FreshMovementState(kFoot);
    const auto refused = Walk(state, 200.f, 3.f); // 200 m/s pendant 3 s
    CHECK(refused >= 10);
    CHECK(ShouldKickForMovement(state, 10));
    CHECK_FALSE(ShouldKickForMovement(state, 0)); // 0 = journal seulement
}

TEST_CASE("Mouvements : NaN ou distance négative = infraction ; 0 = aucune limite")
{
    auto state = FreshMovementState(kFoot);
    CHECK_FALSE(ConsumeMovement(state, std::nanf(""), 0.1f, kFoot));
    CHECK_FALSE(ConsumeMovement(state, -5.f, 0.1f, kFoot));

    MovementState free{};
    CHECK(ConsumeMovement(free, 1e6f, 0.01f, MovementLimits{0.f, 3.f, 60.f}));
}

TEST_CASE("Distances horizontale et 3D")
{
    CHECK(HorizontalDistance({0.f, 0.f, 0.f}, {3.f, 4.f, 100.f}) == Approx(5.f));
    CHECK(Distance({0.f, 0.f, 0.f}, {2.f, 3.f, 6.f}) == Approx(7.f));
}

TEST_CASE("Mouvements : long silence puis position lointaine à vitesse normale (cas réel en véhicule)")
{
    auto state = FreshMovementState(kFoot);
    CHECK(ConsumeMovement(state, 710.f, 83.61f, kFoot)); // 8,5 m/s : vu en jeu le 05/11
    CHECK(state.Strikes == 0.f);
    // Se taire ne donne pas plus que MaxSpeed en moyenne : 60 m/s sur la même durée reste une infraction
    CHECK_FALSE(ConsumeMovement(state, 5017.f, 83.61f, kFoot));
}
