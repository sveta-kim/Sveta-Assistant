#pragma once

#include <string>
#include <unordered_map>

namespace sveta::audio {

// Loaded from config/tts_config.json (tracked, no secrets — just the
// per-language Chirp 3: HD voice name, user-editable to swap in any of
// the 30 available voice personalities). Authentication is a separate
// concern; see GoogleServiceAccount/GoogleOAuthTokenProvider.
struct GoogleTtsConfig {
    // Keyed by audio::ToString(Language), e.g. "Korean" -> "ko-KR-Chirp3-HD-Kore".
    std::unordered_map<std::string, std::string> voicesByLanguage;

    static GoogleTtsConfig Load();
};

} // namespace sveta::audio
