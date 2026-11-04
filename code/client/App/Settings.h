#pragma once

namespace fs = std::filesystem;

struct Settings
{
    static Settings& Get()
    {
        static Settings instance;
        return instance;
    }
    static bool IsDisabled()
    {
        return !Get().enabled;
    }
    static void Load();

    fs::path exePath{};
    fs::path gamePath{};
    String Version{};
    String ip = "127.0.0.1";
    uint16_t port = 11778;
    // Pseudo envoyé au serveur (--username=<pseudo>). Validé côté serveur.
    String username = "testuser";
    // Jeton de session remis par le launcher (--session=<jeton>). Jamais journalisé.
    String sessionToken{};
    // Vrai si le jeu a été lancé par le launcher (--session= présent), même après usage du jeton.
    bool launchedByLauncher = false;
    // Relais local du launcher (--launcher=<port> --launcher-key=<clé>) : jeton neuf pour « Rejoindre ».
    uint16_t relayPort = 0;
    std::string relayKey{};
    Vector<fs::path> mods = {};
    bool enabled = false;
    bool RpcOnly = false;
    fs::path RpcPath{};

private:
    Settings() = default;
};