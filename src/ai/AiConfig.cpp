#include "ai/AiConfig.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"

namespace sveta::ai {

namespace {

constexpr const char* kPlaceholderModel = "REPLACE_WITH_MODEL_NAME";
constexpr const char* kPlaceholderKey = "REPLACE_WITH_YOUR_API_KEY";

std::optional<nlohmann::json> LoadJsonFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        core::Logger::Warn("Missing config file: " + path.string());
        return std::nullopt;
    }
    try {
        nlohmann::json parsed;
        file >> parsed;
        return parsed;
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error("Failed to parse " + path.string() + ": " + e.what());
        return std::nullopt;
    }
}

} // namespace

bool AiConfig::IsUsable() const {
    return !endpoint.empty() && !model.empty() && model != kPlaceholderModel &&
           !apiKey.empty() && apiKey != kPlaceholderKey;
}

std::optional<AiConfig> AiConfig::Load() {
    const std::filesystem::path configDir(SVETA_CONFIG_DIR);

    const auto aiConfigJson = LoadJsonFile(configDir / "ai_config.json");
    const auto secretsJson = LoadJsonFile(configDir / "secrets.local.json");
    if (!aiConfigJson || !secretsJson) {
        return std::nullopt;
    }

    AiConfig config;
    config.endpoint = aiConfigJson->value("endpoint", "");
    config.model = aiConfigJson->value("model", "");
    config.apiKey = secretsJson->value("api_key", "");

    if (!config.IsUsable()) {
        core::Logger::Warn(
            "AI config present but not filled in yet (endpoint/model/api_key) — "
            "edit config/ai_config.json and config/secrets.local.json");
    }

    return config;
}

} // namespace sveta::ai
