#pragma once

#include <string>

namespace sveta::audio {

// Loaded from config/google_service_account.json -- the JSON key file
// downloaded from Google Cloud Console (IAM & Admin > Service Accounts >
// pick or create one > Keys tab > Add Key > Create new key > JSON).
// Gitignored; never committed. This project's Google Cloud project has
// API key creation disabled by org policy, so this "Application Default
// Credentials"-style service account key is the only auth path available
// for Cloud Text-to-Speech -- see GoogleOAuthTokenProvider for how it's
// actually used (signs a JWT, exchanges it for a short-lived access
// token; there's no static key to just attach to a request).
struct GoogleServiceAccount {
    std::string clientEmail;
    std::string privateKeyPem; // PKCS#8 PEM, "-----BEGIN PRIVATE KEY-----..."
    std::string tokenUri;      // from the key file; normally https://oauth2.googleapis.com/token

    bool IsUsable() const;
    static GoogleServiceAccount Load();
};

} // namespace sveta::audio
