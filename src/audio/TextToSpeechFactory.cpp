#include "audio/TextToSpeechFactory.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "audio/GoogleServiceAccount.h"
#include "audio/GoogleTextToSpeech.h"
#include "audio/GoogleTtsConfig.h"
#include "audio/TextToSpeech.h"
#include "core/Logger.h"

namespace sveta::audio {

namespace {

std::string LoadProvider() {
    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "tts_config.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        return "sapi";
    }
    try {
        nlohmann::json parsed;
        file >> parsed;
        return parsed.value("provider", "sapi");
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse tts_config.json: ") + e.what());
        return "sapi";
    }
}

} // namespace

std::unique_ptr<ITextToSpeech> CreateTextToSpeech(HWND notifyWindow, UINT notifyMessage) {
    if (LoadProvider() == "google") {
        if (auto google = GoogleTextToSpeech::Create(
                notifyWindow, notifyMessage, GoogleTtsConfig::Load(), GoogleServiceAccount::Load())) {
            core::Logger::Info("TextToSpeech: using Google Cloud TTS (Chirp 3: HD)");
            return google;
        }
        core::Logger::Warn(
            "TextToSpeech: provider is \"google\" but config/google_service_account.json is missing or "
            "incomplete; falling back to local SAPI voices");
    }

    return TextToSpeech::Create(notifyWindow, notifyMessage);
}

} // namespace sveta::audio
