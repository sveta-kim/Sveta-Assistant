#include "audio/GoogleTtsConfig.h"

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
        const auto voices = parsed.value("google_voices", nlohmann::json::object());
        for (auto it = voices.begin(); it != voices.end(); ++it) {
            config.voicesByLanguage[it.key()] = it.value().get<std::string>();
        }
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse tts_config.json: ") + e.what());
    }

    return config;
}

} // namespace sveta::audio
