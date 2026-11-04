#include "LauncherRelay.h"

#include "App/Settings.h"

#include <windows.h>
#include <winhttp.h>

namespace
{
constexpr DWORD kTimeoutMs = 8000;
constexpr size_t kMaxBody = 1024;

struct HInternet
{
    HINTERNET h{nullptr};
    explicit HInternet(HINTERNET aHandle) : h(aHandle) {}
    ~HInternet()
    {
        if (h)
            WinHttpCloseHandle(h);
    }
    HInternet(const HInternet&) = delete;
    HInternet& operator=(const HInternet&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

// Jeton plausible : ASCII imprimable sans espace, 1 à 256 caractères (comme côté serveur).
bool IsPlausibleToken(const std::string& aToken)
{
    if (aToken.empty() || aToken.size() > 256)
        return false;
    for (const char c : aToken)
    {
        if (c <= 0x20 || c >= 0x7F)
            return false;
    }
    return true;
}
} // namespace

namespace LauncherRelay
{
bool IsAvailable()
{
    const auto& settings = Settings::Get();
    return settings.relayPort != 0 && !settings.relayKey.empty();
}

bool FetchSessionToken(std::string& aMessage)
{
    if (!IsAvailable())
    {
        aMessage = "launcher relay not configured";
        return false;
    }
    auto& settings = Settings::Get();

    HInternet session(WinHttpOpen(L"CyberpunkMP-Client", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
    {
        aMessage = "WinHttpOpen failed";
        return false;
    }
    WinHttpSetTimeouts(session.h, kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    // Toujours la boucle locale : la clé ne doit jamais partir ailleurs
    HInternet connection(WinHttpConnect(session.h, L"127.0.0.1", settings.relayPort, 0));
    HInternet request(connection ? WinHttpOpenRequest(connection.h, L"POST", L"/session", nullptr, WINHTTP_NO_REFERER,
                                                      WINHTTP_DEFAULT_ACCEPT_TYPES, 0)
                                 : nullptr);
    if (!request)
    {
        aMessage = "cannot open request";
        return false;
    }

    const std::string header = "X-Launcher-Key: " + settings.relayKey + "\r\n";
    const std::wstring wideHeader(header.begin(), header.end()); // clé hexadécimale : ASCII
    if (!WinHttpSendRequest(request.h, wideHeader.c_str(), static_cast<DWORD>(wideHeader.size()),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr))
    {
        aMessage = "launcher unreachable (closed?)";
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

    std::string body;
    char buffer[256];
    DWORD read = 0;
    while (body.size() < kMaxBody && WinHttpReadData(request.h, buffer, sizeof(buffer), &read) && read > 0)
        body.append(buffer, read);

    if (status != 200)
    {
        aMessage = "launcher refused (HTTP " + std::to_string(status) + ")";
        spdlog::warn("Launcher relay: {} - {}", aMessage, body.substr(0, 200));
        return false;
    }
    if (!IsPlausibleToken(body))
    {
        aMessage = "invalid token from launcher";
        return false;
    }

    settings.sessionToken = body.c_str();
    spdlog::info("Launcher relay: fresh session token received");
    return true;
}
} // namespace LauncherRelay
