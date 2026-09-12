#include "context/PrivacyConfig.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::context {

PrivacyConfig PrivacyConfig::Load() {
    PrivacyConfig config;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "privacy_config.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        core::Logger::Warn("Missing config/privacy_config.json; defaulting to screen awareness ON, no exclusions");
        return config;
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        config.screenAwarenessEnabled = parsed.value("screen_awareness_enabled", true);
        for (const auto& entry : parsed.value("excluded_processes", nlohmann::json::array())) {
            config.excludedProcesses.push_back(core::Utf8ToWide(entry.get<std::string>()));
        }
        config.proactiveSpeechEnabled = parsed.value("proactive_speech_enabled", true);
        config.gameDetectionEnabled = parsed.value("game_detection_enabled", true);
        config.memoryEnabled = parsed.value("memory_enabled", true);
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse privacy_config.json: ") + e.what());
    }

    return config;
}

void PrivacyConfig::Save() const {
    nlohmann::json out;
    out["screen_awareness_enabled"] = screenAwarenessEnabled;
    nlohmann::json excluded = nlohmann::json::array();
    for (const auto& process : excludedProcesses) {
        excluded.push_back(core::WideToUtf8(process));
    }
    out["excluded_processes"] = excluded;
    out["proactive_speech_enabled"] = proactiveSpeechEnabled;
    out["game_detection_enabled"] = gameDetectionEnabled;
    out["memory_enabled"] = memoryEnabled;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "privacy_config.json";
    std::ofstream file(path);
    if (!file.is_open()) {
        core::Logger::Error("Failed to open privacy_config.json for writing");
        return;
    }
    file << out.dump(4);
}

} // namespace sveta::context
