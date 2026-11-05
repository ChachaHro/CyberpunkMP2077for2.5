#include <catch2/catch.hpp>

#include "Security/RateLimit.h"

using namespace RateLimit;

namespace
{
const BucketLimits kChat = kDefaultLimits[static_cast<size_t>(MessageKind::Chat)];

// aCount messages espacés de aInterval secondes ; renvoie le nombre de messages acceptés
int Send(Bucket& aBucket, const BucketLimits& aLimits, int aCount, float aInterval, float aScale = 1.f)
{
    int accepted = 0;
    for (int i = 0; i < aCount; ++i)
        if (TryConsume(aBucket, aLimits, i == 0 ? 0.f : aInterval, aScale))
            ++accepted;
    return accepted;
}
} // namespace

TEST_CASE("Chat : 5 messages d'affilée, puis le surplus est ignoré")
{
    Bucket bucket;
    CHECK(Send(bucket, kChat, 20, 0.f) == 5);
}

TEST_CASE("Chat : un message par seconde passe toujours")
{
    Bucket bucket;
    CHECK(Send(bucket, kChat, 120, 1.f) == 120);
}

TEST_CASE("Déplacements : 20 par seconde (personnage + véhicule à 10 Hz) passent toujours")
{
    Bucket bucket;
    const auto moves = kDefaultLimits[static_cast<size_t>(MessageKind::MoveEntity)];
    CHECK(Send(bucket, moves, 2000, 0.05f) == 2000);
}

TEST_CASE("Échelle : 0 = aucune limite, 2 = limites doublées")
{
    Bucket unlimited;
    CHECK(Send(unlimited, kChat, 1000, 0.f, 0.f) == 1000);
    Bucket doubled;
    CHECK(Send(doubled, kChat, 100, 0.f, 2.f) == 10);
}

TEST_CASE("Expulsion : seulement après beaucoup de messages ignorés d'affilée")
{
    Offences offences;
    bool kicked = false;
    for (int i = 0; i < 99 && !kicked; ++i)
        kicked = RecordDrop(offences, 0.f, 10.f, 100);
    CHECK_FALSE(kicked);
    CHECK(RecordDrop(offences, 0.f, 10.f, 100));
}

TEST_CASE("Expulsion : quelques messages ignorés de temps en temps ne mènent à rien")
{
    Offences offences;
    for (int i = 0; i < 1000; ++i)
        CHECK_FALSE(RecordDrop(offences, 1.f, 10.f, 100));
    Offences never;
    for (int i = 0; i < 1000; ++i)
        CHECK_FALSE(RecordDrop(never, 0.f, 10.f, 0));
}

TEST_CASE("Types de message connus")
{
    CHECK(IsKnownKind(0));
    CHECK(IsKnownKind(6));
    CHECK_FALSE(IsKnownKind(7));
    CHECK_FALSE(IsKnownKind(~0ULL));
}
