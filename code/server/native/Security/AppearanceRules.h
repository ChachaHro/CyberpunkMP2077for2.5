#pragma once

// Validation d'une apparence envoyée par un plugin (Player.SetAppearance), en fonctions pures testées dans
// code/server/tests. L'état de personnalisation est rejoué par le jeu des AUTRES clients : il doit rester borné.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace AppearanceRules
{
inline constexpr std::size_t kMaxTextBytes = 128 * 1024;
inline constexpr std::size_t kMaxCcstateBytes = 32 * 1024;
inline constexpr std::size_t kMaxItems = 64;
inline constexpr std::size_t kMaxItemChars = 128;

// Hexadécimal (minuscules ou majuscules), longueur paire, au plus kMaxCcstateBytes octets décodés.
inline bool DecodeHex(std::string_view aText, std::vector<uint8_t>& aOut)
{
    aOut.clear();
    if (aText.size() % 2 != 0 || aText.size() / 2 > kMaxCcstateBytes)
        return false;

    const auto nibble = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    aOut.reserve(aText.size() / 2);
    for (std::size_t i = 0; i < aText.size(); i += 2)
    {
        const int high = nibble(aText[i]);
        const int low = nibble(aText[i + 1]);
        if (high < 0 || low < 0)
        {
            aOut.clear();
            return false;
        }
        aOut.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

// Nom d'objet (TweakDBID sous forme texte : « Items.Xyz » ou « <TDBID:…:…> »).
inline bool IsValidItemName(std::string_view aName) noexcept
{
    if (aName.empty() || aName.size() > kMaxItemChars)
        return false;
    for (const char c : aName)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                        c == '.' || c == '-' || c == '<' || c == '>' || c == ':';
        if (!ok)
            return false;
    }
    return true;
}
} // namespace AppearanceRules
