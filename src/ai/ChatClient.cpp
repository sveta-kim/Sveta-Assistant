#include "ai/ChatClient.h"

#include <windows.h>
#include <winhttp.h>

#include <format>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::ai {

namespace {

constexpr DWORD kTimeoutMs = 30000;

// RAII wrapper so every early-return path still closes the handle —
// WinHTTP has no owning smart-pointer type of its own.
class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~WinHttpHandle() {
        if (handle_) {
            WinHttpCloseHandle(handle_);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    HINTERNET Get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_;
};

struct ParsedUrl {
    std::wstring host;
    std::wstring path; // path + query string
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool isHttps = true;
};

std::optional<ParsedUrl> ParseUrl(const std::wstring& url) {
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[2048]{};

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = static_cast<DWORD>(std::size(host));
    components.lpszUrlPath = path;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    components.lpszExtraInfo = extra;
    components.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        return std::nullopt;
    }

    ParsedUrl parsed;
    parsed.host = host;
    parsed.path = std::wstring(path) + extra;
    parsed.port = components.nPort;
    parsed.isHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
    return parsed;
}

ChatResult Fail(std::string message) {
    core::Logger::Error("ChatClient: " + message);
    return ChatResult{false, std::move(message)};
}

} // namespace

ChatClient::ChatClient(AiConfig config) : config_(std::move(config)) {}

ChatResult ChatClient::Send(const std::vector<ChatMessage>& history) const {
    if (!config_.IsUsable()) {
        return Fail("AI가 아직 설정되지 않았어요 (config/ai_config.json, config/secrets.local.json 확인 필요)");
    }

    const auto url = ParseUrl(core::Utf8ToWide(config_.endpoint));
    if (!url) {
        return Fail("설정된 endpoint URL을 해석할 수 없어요: " + config_.endpoint);
    }

    nlohmann::json body;
    body["model"] = config_.model;
    body["messages"] = nlohmann::json::array();
    for (const auto& message : history) {
        body["messages"].push_back({{"role", message.role}, {"content", message.content}});
    }
    const std::string bodyStr = body.dump();

    core::Logger::Info(std::format(
        "ChatClient: POST {} ({} messages)", config_.endpoint, history.size()));

    WinHttpHandle session(WinHttpOpen(
        L"SvetaAssistant/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        return Fail(std::format("WinHttpOpen failed (error={})", GetLastError()));
    }
    WinHttpSetTimeouts(session.Get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    WinHttpHandle connect(WinHttpConnect(session.Get(), url->host.c_str(), url->port, 0));
    if (!connect) {
        return Fail(std::format("WinHttpConnect failed (error={})", GetLastError()));
    }

    const DWORD requestFlags = url->isHttps ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(
        connect.Get(), L"POST", url->path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags));
    if (!request) {
        return Fail(std::format("WinHttpOpenRequest failed (error={})", GetLastError()));
    }

    const std::wstring headers =
        L"Content-Type: application/json\r\nAuthorization: Bearer " + core::Utf8ToWide(config_.apiKey) + L"\r\n";

    const BOOL sent = WinHttpSendRequest(
        request.Get(), headers.c_str(), static_cast<DWORD>(-1L),
        const_cast<char*>(bodyStr.data()), static_cast<DWORD>(bodyStr.size()),
        static_cast<DWORD>(bodyStr.size()), 0);
    if (!sent) {
        return Fail(std::format("WinHttpSendRequest failed (error={})", GetLastError()));
    }
    const BOOL received = WinHttpReceiveResponse(request.Get(), nullptr);
    if (!received) {
        return Fail(std::format("WinHttpReceiveResponse failed (error={})", GetLastError()));
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request.Get(), WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    core::Logger::Info(std::format("ChatClient: status code {}", statusCode));

    std::string responseBody;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request.Get(), &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.Get(), chunk.data(), available, &bytesRead)) {
            break;
        }
        chunk.resize(bytesRead);
        responseBody += chunk;
    }

    if (statusCode != 200) {
        return Fail(std::format("HTTP {} — {}", statusCode, responseBody.substr(0, 300)));
    }

    try {
        const auto parsed = nlohmann::json::parse(responseBody);
        if (parsed.contains("error")) {
            return Fail("API error: " + parsed["error"].dump());
        }
        const std::string content = parsed.at("choices").at(0).at("message").at("content").get<std::string>();
        return ChatResult{true, content};
    } catch (const nlohmann::json::exception& e) {
        return Fail(std::string("Failed to parse response: ") + e.what());
    }
}

} // namespace sveta::ai
