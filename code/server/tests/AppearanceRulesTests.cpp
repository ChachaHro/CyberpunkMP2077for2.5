#include <catch2/catch.hpp>

#include "Security/AppearanceRules.h"

using namespace AppearanceRules;

TEST_CASE("Apparence : hexadécimal décodé")
{
    std::vector<uint8_t> out;
    CHECK(DecodeHex("00ff10Ab", out));
    CHECK(out == std::vector<uint8_t>{0x00, 0xFF, 0x10, 0xAB});
    CHECK(DecodeHex("", out));
    CHECK(out.empty());
}

TEST_CASE("Apparence : hexadécimal forgé refusé")
{
    std::vector<uint8_t> out;
    CHECK_FALSE(DecodeHex("abc", out));      // longueur impaire
    CHECK_FALSE(DecodeHex("zz", out));       // pas de l'hexadécimal
    CHECK_FALSE(DecodeHex("0g", out));
    CHECK(out.empty());
    CHECK_FALSE(DecodeHex(std::string((kMaxCcstateBytes + 1) * 2, 'a'), out)); // trop gros
    CHECK(DecodeHex(std::string(kMaxCcstateBytes * 2, 'a'), out));
}

TEST_CASE("Apparence : noms d'objets")
{
    CHECK(IsValidItemName("Items.Q005_Johnny_Shirt"));
    CHECK(IsValidItemName("<TDBID:12345678:2A>"));
    CHECK(IsValidItemName("Items.Some-Item_01"));
    CHECK_FALSE(IsValidItemName(""));
    CHECK_FALSE(IsValidItemName(std::string(kMaxItemChars + 1, 'a')));
    CHECK_FALSE(IsValidItemName("Items.X\nforgé"));
    CHECK_FALSE(IsValidItemName("Items X"));
    CHECK_FALSE(IsValidItemName("Items.\"X\""));
}
