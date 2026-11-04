#include "Settings.h"
#include <RED4ext/LaunchParameters.hpp>

void Settings::Load()
{
    Settings& settings = Get();

    auto& launchParameters = RED4ext::GetLaunchParameters();

    if (launchParameters.Contains(RED4ext::CString("-online")))
        settings.enabled = true;

    bool ipFromArgs = false;
    bool portFromArgs = false;

    if (const auto ip = launchParameters.Get("-ip"); ip)
    {
        if (ip->size() > 0)
        {
            settings.ip = (*ip)[0].c_str();
            ipFromArgs = true;
        }
    }

    if (const auto port = launchParameters.Get("-port"); port)
    {
        if (port->size() > 0)
        {
            settings.port = std::strtoul((*port)[0].c_str(), nullptr, 10) & 0xFFFF;
            portFromArgs = true;
        }
    }

    // Même forme que --ip= : "--username=<pseudo>". Sans lui, tous les joueurs seraient "testuser".
    if (const auto username = launchParameters.Get("-username"); username)
    {
        if (username->size() > 0)
            settings.username = (*username)[0].c_str();
    }
    spdlog::info("Username: {}", settings.username);

    // Jeton du launcher : "--session=<jeton>". On ne journalise que sa présence, jamais sa valeur.
    if (const auto session = launchParameters.Get("-session"); session)
    {
        if (session->size() > 0)
        {
            settings.sessionToken = (*session)[0].c_str();
            settings.launchedByLauncher = true;
        }
    }
    spdlog::info("Session token: {}", settings.sessionToken.empty() ? "none" : "provided");

    // Relais du launcher : port local + clé (jamais journalisée). Ignoré si l'un des deux manque.
    if (const auto relay = launchParameters.Get("-launcher"); relay && relay->size() > 0)
    {
        const auto port = std::strtoul((*relay)[0].c_str(), nullptr, 10);
        if (port > 0 && port <= 0xFFFF)
            settings.relayPort = static_cast<uint16_t>(port);
    }
    if (const auto key = launchParameters.Get("-launcher-key"); key && key->size() > 0)
    {
        const std::string value = (*key)[0].c_str();
        if (value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos)
            settings.relayKey = value;
    }
    spdlog::info("Launcher relay: {}", settings.relayPort && !settings.relayKey.empty() ? "available" : "none");

    // Report what we actually parsed. The game's parser only fills these in for the
    // --ip=<addr> / --port=<n> form; "-ip <addr>" produces an empty value list and
    // silently leaves the defaults in place, which looks identical to a dead server.
    spdlog::info("Server address: {}:{} (ip {}, port {})", settings.ip, settings.port,
                 ipFromArgs ? "from launch args" : "DEFAULT - --ip= was not parsed",
                 portFromArgs ? "from launch args" : "DEFAULT - --port= was not parsed");

    if (const auto mods = launchParameters.Get("-mod"); mods)
    {
        for (const auto& mod : *mods)
            settings.mods.push_back(mod.c_str());
    }

    if (launchParameters.Contains("-rpc"))
    {
        settings.RpcOnly = true;
    }

    if (const auto rpcDir = launchParameters.Get("-rpcdir"); rpcDir)
    {
        if (rpcDir->size() > 0)
        {
            // For some reason cyberpunk adds a \ at the start and end of the path...
            std::string path = std::string((*rpcDir)[0].c_str());
            path = path.substr(1, path.length() - 2);

            settings.RpcPath = path;
        }
    }
}
