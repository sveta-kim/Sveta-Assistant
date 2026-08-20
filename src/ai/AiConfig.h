#pragma once

#include <optional>
#include <string>

namespace sveta::ai {

// Loaded from config/ai_config.json (tracked) and config/secrets.local.json
// (gitignored — the user fills in their own key; see
// config/secrets.local.example.json for the shape).
struct AiConfig {
    std::string endpoint;
    std::string model;
    std::string apiKey;

    // False if the endpoint/model/key still look like placeholders or a
    // config file is missing/malformed — callers should show a friendly
    // "not configured" message instead of attempting a network call.
    bool IsUsable() const;

    static std::optional<AiConfig> Load();
};

} // namespace sveta::ai
