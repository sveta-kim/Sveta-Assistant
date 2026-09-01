#include "audio/GoogleTextToSpeech.h"

#include <mmsystem.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "audio/LanguageDetection.h"
#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::audio {

namespace {

constexpr DWORD kTimeoutMs = 15000;
constexpr wchar_t kHost[] = L"texttospeech.googleapis.com";
constexpr wchar_t kPath[] = L"/v1/text:synthesize";
constexpr int kFallbackDurationMs = 3000; // used only if the WAV header can't be parsed

// Cloud TTS's audioConfig.volumeGainDb is a gain in decibels (0 = normal,
// negative = quieter), not a percentage — convert using the standard
// amplitude-to-dB formula (50% ~= -6dB, matching what "half as loud"
// perceptually means) so tts_config.json's volume_percent stays intuitive.
double VolumePercentToGainDb(int percent) {
    return 20.0 * std::log10(static_cast<double>(std::clamp(percent, 1, 100)) / 100.0);
}

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

std::optional<std::vector<BYTE>> Base64Decode(const std::string& base64) {
    DWORD size = 0;
    if (!CryptStringToBinaryA(
            base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr,
            nullptr)) {
        return std::nullopt;
    }
    std::vector<BYTE> bytes(size);
    if (!CryptStringToBinaryA(
            base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64, bytes.data(), &size, nullptr,
            nullptr)) {
        return std::nullopt;
    }
    bytes.resize(size);
    return bytes;
}

// Reads the handful of RIFF/WAVE fields needed to compute exact playback
// duration. Google's LINEAR16 responses always include this header.
std::optional<int> ComputeWavDurationMs(const std::vector<BYTE>& wav) {
    if (wav.size() < 44 || std::memcmp(wav.data(), "RIFF", 4) != 0 || std::memcmp(wav.data() + 8, "WAVE", 4) != 0) {
        return std::nullopt;
    }

    const auto readU32 = [&wav](size_t offset) {
        return static_cast<uint32_t>(wav[offset]) | (static_cast<uint32_t>(wav[offset + 1]) << 8) |
            (static_cast<uint32_t>(wav[offset + 2]) << 16) | (static_cast<uint32_t>(wav[offset + 3]) << 24);
    };

    size_t offset = 12;
    uint32_t byteRate = 0;
    while (offset + 8 <= wav.size()) {
        const size_t dataStart = offset + 8;
        const uint32_t chunkSize = readU32(offset + 4);

        if (std::memcmp(&wav[offset], "fmt ", 4) == 0 && dataStart + 16 <= wav.size()) {
            byteRate = readU32(dataStart + 8);
        } else if (std::memcmp(&wav[offset], "data", 4) == 0) {
            const uint32_t dataSize = std::min<uint32_t>(chunkSize, static_cast<uint32_t>(wav.size() - dataStart));
            if (byteRate == 0) {
                return std::nullopt;
            }
            return static_cast<int>((static_cast<uint64_t>(dataSize) * 1000) / byteRate);
        }
        offset = dataStart + chunkSize + (chunkSize % 2); // chunks are word-aligned
    }
    return std::nullopt;
}

std::optional<std::vector<BYTE>> FetchSynthesizedAudio(
    const GoogleTtsConfig& config, const std::string& accessToken, const std::wstring& text) {
    const Language language = DetectLanguage(text);
    const auto voiceIt = config.voicesByLanguage.find(std::string(ToString(language)));
    if (voiceIt == config.voicesByLanguage.end()) {
        core::Logger::Warn(std::format(
            "GoogleTextToSpeech: no voice configured for {} in tts_config.json", ToString(language)));
        return std::nullopt;
    }

    nlohmann::json body;
    body["input"]["text"] = core::WideToUtf8(text);
    body["voice"]["languageCode"] = std::string(GoogleLanguageCode(language));
    body["voice"]["name"] = voiceIt->second;
    body["audioConfig"]["audioEncoding"] = "LINEAR16";
    if (config.volumePercent != 100) {
        body["audioConfig"]["volumeGainDb"] = VolumePercentToGainDb(config.volumePercent);
    }
    const std::string bodyStr = body.dump();

    WinHttpHandle session(WinHttpOpen(
        L"SvetaAssistant/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        return std::nullopt;
    }
    WinHttpSetTimeouts(session.Get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    WinHttpHandle connect(WinHttpConnect(session.Get(), kHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connect) {
        return std::nullopt;
    }

    WinHttpHandle request(WinHttpOpenRequest(
        connect.Get(), L"POST", kPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (!request) {
        return std::nullopt;
    }

    const std::wstring headers =
        L"Content-Type: application/json; charset=utf-8\r\nAuthorization: Bearer " + core::Utf8ToWide(accessToken) +
        L"\r\n";
    if (!WinHttpSendRequest(
            request.Get(), headers.c_str(), static_cast<DWORD>(-1L), const_cast<char*>(bodyStr.data()),
            static_cast<DWORD>(bodyStr.size()), static_cast<DWORD>(bodyStr.size()), 0)) {
        core::Logger::Warn(std::format("GoogleTextToSpeech: WinHttpSendRequest failed (error={})", GetLastError()));
        return std::nullopt;
    }
    if (!WinHttpReceiveResponse(request.Get(), nullptr)) {
        core::Logger::Warn(
            std::format("GoogleTextToSpeech: WinHttpReceiveResponse failed (error={})", GetLastError()));
        return std::nullopt;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request.Get(), WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

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
        core::Logger::Warn(std::format("GoogleTextToSpeech: HTTP {} — {}", statusCode, responseBody.substr(0, 300)));
        return std::nullopt;
    }

    try {
        const auto parsed = nlohmann::json::parse(responseBody);
        return Base64Decode(parsed.at("audioContent").get<std::string>());
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Warn(std::string("GoogleTextToSpeech: failed to parse response: ") + e.what());
        return std::nullopt;
    }
}

} // namespace

std::unique_ptr<GoogleTextToSpeech> GoogleTextToSpeech::Create(
    HWND notifyWindow, UINT notifyMessage, GoogleTtsConfig config, GoogleServiceAccount account) {
    if (!account.IsUsable()) {
        return nullptr;
    }
    return std::unique_ptr<GoogleTextToSpeech>(
        new GoogleTextToSpeech(notifyWindow, notifyMessage, std::move(config), std::move(account)));
}

GoogleTextToSpeech::GoogleTextToSpeech(
    HWND notifyWindow, UINT notifyMessage, GoogleTtsConfig config, GoogleServiceAccount account)
    : notifyWindow_(notifyWindow),
      notifyMessage_(notifyMessage),
      config_(std::move(config)),
      tokenProvider_(std::move(account)) {}

GoogleTextToSpeech::~GoogleTextToSpeech() {
    Stop();
}

void GoogleTextToSpeech::Speak(const std::wstring& text) {
    if (text.empty()) {
        return;
    }

    const int myGeneration = ++generation_;
    const GoogleTtsConfig config = config_;

    std::thread([this, myGeneration, text, config]() {
        const auto accessToken = tokenProvider_.GetAccessToken();
        if (!accessToken || myGeneration != generation_.load()) {
            return; // couldn't authenticate, or superseded while we were waiting
        }

        const auto wavBytes = FetchSynthesizedAudio(config, *accessToken, text);
        if (!wavBytes || myGeneration != generation_.load()) {
            return; // fetch failed, or superseded by a newer Speak()/Stop() while we were waiting
        }

        PlaySoundW(reinterpret_cast<LPCWSTR>(wavBytes->data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
        SetPending(/*started=*/true, /*ended=*/false);
        PostMessageW(notifyWindow_, notifyMessage_, 0, 0);

        const int durationMs = ComputeWavDurationMs(*wavBytes).value_or(kFallbackDurationMs);
        Sleep(static_cast<DWORD>(durationMs));

        if (myGeneration == generation_.load()) {
            SetPending(/*started=*/false, /*ended=*/true);
            PostMessageW(notifyWindow_, notifyMessage_, 0, 0);
        }
    }).detach();
}

void GoogleTextToSpeech::Stop() {
    ++generation_; // invalidate any in-flight fetch/playback thread's completion
    PlaySoundW(nullptr, nullptr, 0);
    SetPending(/*started=*/false, /*ended=*/true);
    PostMessageW(notifyWindow_, notifyMessage_, 0, 0);
}

void GoogleTextToSpeech::SetPending(bool started, bool ended) {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pendingStarted_ = pendingStarted_ || started;
    pendingEnded_ = pendingEnded_ || ended;
}

ITextToSpeech::EventResult GoogleTextToSpeech::PumpEvents() {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    const EventResult result{pendingStarted_, pendingEnded_};
    pendingStarted_ = false;
    pendingEnded_ = false;
    return result;
}

} // namespace sveta::audio
