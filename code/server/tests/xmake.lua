-- Tests unitaires des règles serveur en fonctions pures (sans moteur ni réseau).
-- Non compilés par défaut : xmake build Server.Tests && xmake run Server.Tests
target("Server.Tests")
    set_kind("binary")
    set_group("Tests")
    set_default(false)
    add_files("*.cpp")
    add_includedirs("../native")
    add_packages("catch2")
