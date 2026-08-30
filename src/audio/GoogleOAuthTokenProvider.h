#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>

#include "audio/GoogleServiceAccount.h"

namespace sveta::audio {

// Mints and caches short-lived OAuth2 access tokens for Google Cloud API
// calls, using a service account's JSON key (see GoogleServiceAccount) --
// signs a JWT assertion with the account's RSA private key and exchanges
// it for an access token, the standard "self-signed JWT" service-account
// flow. This is the only auth path available here: this project's Google
// Cloud project has API key creation disabled by org policy.
//
// Thread-safe; GetAccessToken() may be called from any thread (in
// particular, GoogleTextToSpeech's background fetch thread), and blocks
// on a network call only when the cached token is missing or close to
// expiring.
class GoogleOAuthTokenProvider {
public:
    explicit GoogleOAuthTokenProvider(GoogleServiceAccount account);

    // nullopt if the service account isn't usable, or the JWT signing /
    // token exchange failed.
    std::optional<std::string> GetAccessToken();

private:
    std::optional<std::string> MintAccessToken() const;

    GoogleServiceAccount account_;

    std::mutex mutex_;
    std::string cachedToken_;
    std::chrono::steady_clock::time_point expiresAt_;
};

} // namespace sveta::audio
