#pragma once

// Règles de connexion du serveur, en fonctions pures (aucune dépendance au moteur) pour être testées
// seules : code/server/tests. Elles servent de garde-fou natif ; les plugins peuvent être plus stricts.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ConnectionRules
{
inline constexpr std::size_t kMaxUsernameCodePoints = 32;
inline constexpr std::size_t kMaxLogFieldBytes = 256;

// Décode un point de code UTF-8 strict à partir de aText[aPos]. Renvoie false si la séquence est
// invalide (tronquée, surlongue, surrogate, au-delà de U+10FFFF).
inline bool DecodeUtf8(std::string_view aText, std::size_t& aPos, char32_t& aCodePoint) noexcept
{
    const auto byte = [&](std::size_t i) { return static_cast<uint8_t>(aText[i]); };
    const uint8_t lead = byte(aPos);
    std::size_t length;
    char32_t value;
    if (lead < 0x80) { aCodePoint = lead; aPos += 1; return true; }
    if ((lead & 0xE0) == 0xC0) { length = 2; value = lead & 0x1F; }
    else if ((lead & 0xF0) == 0xE0) { length = 3; value = lead & 0x0F; }
    else if ((lead & 0xF8) == 0xF0) { length = 4; value = lead & 0x07; }
    else return false;

    if (aPos + length > aText.size())
        return false;
    for (std::size_t i = 1; i < length; ++i)
    {
        const uint8_t next = byte(aPos + i);
        if ((next & 0xC0) != 0x80)
            return false;
        value = (value << 6) | (next & 0x3F);
    }

    constexpr char32_t kMinByLength[] = {0, 0, 0x80, 0x800, 0x10000};
    if (value < kMinByLength[length] || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return false;

    aCodePoint = value;
    aPos += length;
    return true;
}

// Caractères invisibles ou trompeurs : contrôles C0/C1, DEL, largeur nulle, marques et contrôles bidi, BOM.
inline bool IsForbiddenCodePoint(char32_t c) noexcept
{
    return c < 0x20 || c == 0x7F || (c >= 0x80 && c <= 0x9F) || (c >= 0x200B && c <= 0x200F) ||
           (c >= 0x202A && c <= 0x202E) || (c >= 0x2060 && c <= 0x2069) || c == 0xFEFF;
}

// Pseudo réseau : UTF-8 valide, 1 à 32 caractères, sans caractère invisible, sans espace au début ni à
// la fin. Volontairement plus large que la règle typique d'un plugin (lettres, chiffres, espace, _ . -)
// pour ne jamais refuser un compte que le plugin accepte.
inline bool IsValidUsername(std::string_view aUsername) noexcept
{
    if (aUsername.empty() || aUsername.front() == ' ' || aUsername.back() == ' ')
        return false;

    std::size_t count = 0;
    for (std::size_t pos = 0; pos < aUsername.size();)
    {
        char32_t c;
        if (!DecodeUtf8(aUsername, pos, c) || IsForbiddenCodePoint(c))
            return false;
        if (++count > kMaxUsernameCodePoints)
            return false;
    }
    return true;
}

// Deux pseudos désignent-ils le même joueur ? Comparaison insensible à la casse ASCII.
inline bool SameUsername(std::string_view aLeft, std::string_view aRight) noexcept
{
    if (aLeft.size() != aRight.size())
        return false;
    const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    for (std::size_t i = 0; i < aLeft.size(); ++i)
    {
        if (lower(aLeft[i]) != lower(aRight[i]))
            return false;
    }
    return true;
}

// Serveur plein ? MaxPlayer = 0 signifie « aucune limite ».
inline bool IsServerFull(uint32_t aConnectedPlayers, uint32_t aMaxPlayers) noexcept
{
    return aMaxPlayers != 0 && aConnectedPlayers >= aMaxPlayers;
}

// Texte venu d'un client, rendu sûr pour une ligne de journal : caractères invisibles et UTF-8 invalide
// remplacés par '?', coupé à aMaxBytes sans couper un caractère.
inline std::string SanitizeForLog(std::string_view aText, std::size_t aMaxBytes = kMaxLogFieldBytes)
{
    std::string out;
    out.reserve(aText.size() < aMaxBytes ? aText.size() : aMaxBytes);
    for (std::size_t pos = 0; pos < aText.size();)
    {
        const std::size_t start = pos;
        char32_t c;
        const bool valid = DecodeUtf8(aText, pos, c);
        if (!valid)
            pos = start + 1;

        const bool keep = valid && !IsForbiddenCodePoint(c);
        const std::size_t size = keep ? pos - start : 1;
        if (out.size() + size > aMaxBytes)
        {
            out += "...";
            break;
        }
        if (keep)
            out.append(aText.substr(start, size));
        else
            out += '?';
    }
    return out;
}
} // namespace ConnectionRules
