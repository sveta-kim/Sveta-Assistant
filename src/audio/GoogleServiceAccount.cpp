#include "audio/GoogleServiceAccount.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/Logger.h"

namespace sveta::audio {

bool GoogleServiceAccount::IsUsable() const {
    return !clientEmail.empty() && !privateKeyPem.empty() && !tokenUri.empty();
}

GoogleServiceAccount GoogleServiceAccount::Load() {
    GoogleServiceAccount account;

    const std::filesystem::path path = std::filesystem::path(SVETA_CONFIG_DIR) / "google_service_account.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        return account; // not configured -- normal until the user drops the key file in
    }

    try {
        nlohmann::json parsed;
        file >> parsed;
        account.clientEmail = parsed.value("client_email", "");
        account.privateKeyPem = parsed.value("private_key", "");
        account.tokenUri = parsed.value("token_uri", "https://oauth2.googleapis.com/token");
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Error(std::string("Failed to parse google_service_account.json: ") + e.what());
    }

    return account;
}

} // namespace sveta::audio
