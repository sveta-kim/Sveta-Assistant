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
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse privacy_config.json: ") + e.what());
    }

    return config;
}

} // namespace sveta::context
