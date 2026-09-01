#include "ai/UserProfile.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"

namespace sveta::ai {

UserProfile UserProfile::Load() {
    UserProfile profile;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "user_profile.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        return profile;
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        profile.userName = parsed.value("user_name", std::string());
        profile.relationshipNote = parsed.value("relationship_note", std::string());
        profile.primaryLanguage = parsed.value("primary_language", std::string("Korean"));
        profile.uiLanguage = parsed.value("ui_language", std::string("ko"));
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse user_profile.json: ") + e.what());
    }

    return profile;
}

void UserProfile::Save() const {
    nlohmann::json out;
    out["user_name"] = userName;
    out["relationship_note"] = relationshipNote;
    out["primary_language"] = primaryLanguage;
    out["ui_language"] = uiLanguage;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "user_profile.json";
    std::ofstream file(path);
    if (!file.is_open()) {
        core::Logger::Error("Failed to open user_profile.json for writing");
        return;
    }
    file << out.dump(4);
}

} // namespace sveta::ai
