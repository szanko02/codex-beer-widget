#include "sync_transport.hpp"
#include "settings.hpp"
#include <fstream>
#include <wincrypt.h>
#include <winhttp.h>

namespace beer {
namespace {
struct InternetHandle {
    HINTERNET value = nullptr;
    ~InternetHandle() {
        if (value)
            WinHttpCloseHandle(value);
    }
};
struct LocalBuffer {
    DATA_BLOB value{};
    ~LocalBuffer() {
        if (value.pbData) {
            SecureZeroMemory(value.pbData, value.cbData);
            LocalFree(value.pbData);
        }
    }
};
bool safe_token(const std::string &text, size_t minimum, size_t maximum) {
    return text.size() >= minimum && text.size() <= maximum &&
           text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") ==
               std::string::npos;
}
struct Transport {
    std::filesystem::path path;
    Json config, pending;
    std::wstring host, route, authorization;
    INTERNET_PORT port{};
    explicit Transport(std::filesystem::path file) : path(std::move(file)) {
        std::ifstream input(path, std::ios::binary);
        std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(input)), {});
        if (encrypted.empty() || encrypted.size() > 65536)
            throw std::runtime_error("Invalid sync configuration");
        DATA_BLOB blob{static_cast<DWORD>(encrypted.size()), encrypted.data()};
        LocalBuffer decoded;
        if (!CryptUnprotectData(&blob, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                                &decoded.value))
            throw std::runtime_error("Cannot decrypt sync configuration");
        config = Json::parse(decoded.value.pbData, decoded.value.pbData + decoded.value.cbData);
        const auto origin = config.at("origin").get<std::string>();
        if (origin.size() > 2048 ||
            origin.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.:/[]") != std::string::npos)
            throw std::runtime_error("Invalid sync origin");
        const std::wstring url(origin.begin(), origin.end());
        URL_COMPONENTS parts{sizeof(parts)};
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength =
            parts.dwPasswordLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS ||
            parts.dwUserNameLength || parts.dwPasswordLength || parts.dwExtraInfoLength ||
            (parts.dwUrlPathLength && std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) != L"/"))
            throw std::runtime_error("Sync requires an HTTPS origin");
        host.assign(parts.lpszHostName, parts.dwHostNameLength);
        port = parts.nPort;
        const auto device = config.at("deviceId").get<std::string>();
        const auto secret = config.at("publisherSecret").get<std::string>();
        if (!safe_token(device, 16, 128) || !safe_token(secret, 43, 128))
            throw std::runtime_error("Invalid sync credentials");
        const auto revision = config.at("revision").get<int64_t>();
        if (revision < 0 || revision >= SyncMaxInteger)
            throw std::runtime_error("Invalid persisted revision");
        route = L"/v1/devices/" + std::wstring(device.begin(), device.end()) + L"/state";
        authorization = L"Authorization: Bearer " + std::wstring(secret.begin(), secret.end()) +
                        L"\r\nContent-Type: application/json\r\n";
    }
    void persist() {
        auto text = config.dump();
        DATA_BLOB blob{static_cast<DWORD>(text.size()), reinterpret_cast<BYTE *>(text.data())};
        LocalBuffer encrypted;
        const bool protected_ok = CryptProtectData(&blob, L"Codex quota sync", nullptr, nullptr, nullptr,
                                                   CRYPTPROTECT_UI_FORBIDDEN, &encrypted.value) != FALSE;
        SecureZeroMemory(text.data(), text.size());
        if (!protected_ok)
            throw std::runtime_error("Cannot protect sync configuration");
        auto temp = path;
        temp += L".tmp";
        HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot persist sync revision");
        DWORD written = 0;
        const bool saved =
            WriteFile(file, encrypted.value.pbData, encrypted.value.cbData, &written, nullptr) &&
            written == encrypted.value.cbData && FlushFileBuffers(file);
        CloseHandle(file);
        if (!saved ||
            !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot commit sync revision");
    }
    bool send(Json snapshot) {
        snapshot.erase("revision");
        auto previous = pending;
        if (previous.is_object())
            previous.erase("revision");
        if (snapshot != previous) {
            const auto revision = config.at("revision").get<int64_t>();
            if (revision >= SyncMaxInteger)
                throw std::runtime_error("Sync revision exhausted");
            config["revision"] = revision + 1;
            persist(); // Commit before any request can reach the relay.
            snapshot["revision"] = revision + 1;
            pending = std::move(snapshot);
        }
        InternetHandle session{WinHttpOpen(L"CodexBeerWidget-Sync/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
        if (!session.value || !WinHttpSetTimeouts(session.value, 1000, 1000, 1000, 1000))
            return false;
        InternetHandle connection{WinHttpConnect(session.value, host.c_str(), port, 0)};
        if (!connection.value)
            return false;
        InternetHandle request{WinHttpOpenRequest(connection.value, L"POST", route.c_str(), nullptr,
                                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  WINHTTP_FLAG_SECURE)};
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!request.value ||
            !WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)))
            return false;
        const auto body = pending.dump();
        if (!WinHttpSendRequest(request.value, authorization.c_str(),
                                static_cast<DWORD>(authorization.size()), const_cast<char *>(body.data()),
                                static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) ||
            !WinHttpReceiveResponse(request.value, nullptr))
            return false;
        DWORD status = 0, size = sizeof(status);
        const bool delivered =
            WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) &&
            status >= 200 && status < 300;
        if (delivered)
            pending = Json{};
        return delivered;
    }
};
} // namespace
std::unique_ptr<SyncPublisher> configured_sync_publisher() {
    const auto path = settings_directory() / L"sync.dpapi";
    if (!std::filesystem::exists(path))
        return {};
    auto transport = std::make_shared<Transport>(path);
    return std::make_unique<SyncPublisher>(
        [transport](const Json &snapshot) { return transport->send(snapshot); });
}
} // namespace beer
