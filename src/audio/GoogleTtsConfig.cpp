#include "audio/GoogleTtsConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"

namespace sveta::audio {

GoogleTtsConfig GoogleTtsConfig::Load() {
    GoogleTtsConfig config;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "tts_config.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        return config;
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        config.provider = parsed.value("provider", std::string("sapi"));
        const auto voices = parsed.value("google_voices", nlohmann::json::object());
        for (auto it = voices.begin(); it != voices.end(); ++it) {
            config.voicesByLanguage[it.key()] = it.value().get<std::string>();
        }
        config.volumePercent = std::clamp(parsed.value("volume_percent", 100), 1, 100);
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse tts_config.json: ") + e.what());
    }

    return config;
}

void GoogleTtsConfig::Save() const {
    nlohmann::json out;
    out["provider"] = provider;
    out["volume_percent"] = volumePercent;
    nlohmann::json voices = nlohmann::json::object();
    for (const auto& [language, voice] : voicesByLanguage) {
        voices[language] = voice;
    }
    out["google_voices"] = voices;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "tts_config.json";
    std::ofstream file(path);
    if (!file.is_open()) {
        core::Logger::Error("Failed to open tts_config.json for writing");
        return;
    }
    file << out.dump(4);
}

} // namespace sveta::audio
