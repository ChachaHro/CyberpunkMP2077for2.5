// Point d'entrée explicite : CATCH_CONFIG_MAIN choisit wmain sous Windows en mode UNICODE.
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include "Security/ConnectionRules.h"

using namespace ConnectionRules;

TEST_CASE("Pseudos acceptés : tout pseudo usuel d'un plugin")
{
    CHECK(IsValidUsername("a"));
    CHECK(IsValidUsername("PlayerOne"));
    CHECK(IsValidUsername("Jean-Luc.V_2"));
    CHECK(IsValidUsername("Rémi Éloïse"));
    CHECK(IsValidUsername(std::string(32, 'x')));
    CHECK(IsValidUsername("\xC3\xA9" "\xC3\xA9")); // « éé » en UTF-8
}

TEST_CASE("Pseudos refusés")
{
    CHECK_FALSE(IsValidUsername(""));
    CHECK_FALSE(IsValidUsername(std::string(33, 'x')));
    CHECK_FALSE(IsValidUsername(" espace"));
    CHECK_FALSE(IsValidUsername("espace "));
    CHECK_FALSE(IsValidUsername("ligne\nforgée"));
    CHECK_FALSE(IsValidUsername("tab\tulation"));
    CHECK_FALSE(IsValidUsername("ansi\x1b[31m"));
    CHECK_FALSE(IsValidUsername("del\x7f"));
    CHECK_FALSE(IsValidUsername("zero\xE2\x80\x8Bwidth"));   // U+200B
    CHECK_FALSE(IsValidUsername("bidi\xE2\x80\xAEgnp"));      // U+202E
    CHECK_FALSE(IsValidUsername("bom\xEF\xBB\xBF"));          // U+FEFF
    CHECK_FALSE(IsValidUsername("\xC3"));                     // tronqué
    CHECK_FALSE(IsValidUsername("\xC0\xAF"));                 // surlong
    CHECK_FALSE(IsValidUsername("\xED\xA0\x80"));             // surrogate
    CHECK_FALSE(IsValidUsername("\xF4\x90\x80\x80"));         // > U+10FFFF
    CHECK_FALSE(IsValidUsername(std::string("nul\0x", 5)));
}

TEST_CASE("33 caractères accentués refusés, 32 acceptés (on compte des caractères, pas des octets)")
{
    std::string name;
    for (int i = 0; i < 32; ++i)
        name += "\xC3\xA9";
    CHECK(IsValidUsername(name));
    name += "\xC3\xA9";
    CHECK_FALSE(IsValidUsername(name));
}

TEST_CASE("Doublons de pseudo insensibles à la casse")
{
    CHECK(SameUsername("Alice", "alice"));
    CHECK(SameUsername("ALICE", "alice"));
    CHECK_FALSE(SameUsername("Alice", "Alice2"));
    CHECK_FALSE(SameUsername("Alice", "Alica"));
}

TEST_CASE("Serveur plein")
{
    CHECK_FALSE(IsServerFull(0, 32));
    CHECK_FALSE(IsServerFull(31, 32));
    CHECK(IsServerFull(32, 32));
    CHECK(IsServerFull(40, 32));
    CHECK_FALSE(IsServerFull(1000, 0)); // 0 = sans limite
}

TEST_CASE("Journal : texte client nettoyé et borné")
{
    CHECK(SanitizeForLog("bonjour") == "bonjour");
    CHECK(SanitizeForLog("Éloïse") == "Éloïse");
    CHECK(SanitizeForLog("a\nb\rc") == "a?b?c");
    CHECK(SanitizeForLog("x\x1b[2J") == "x?[2J");
    CHECK(SanitizeForLog("bad\xC3") == "bad?");
    CHECK(SanitizeForLog(std::string(10, 'a'), 4) == "aaaa...");
    // Ne coupe jamais au milieu d'un caractère multi-octets
    CHECK(SanitizeForLog("aé", 2) == "a...");
}

int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}
