#pragma once

#include <string>
#include <unordered_map>

namespace sveta::audio {

// Loaded from config/tts_config.json (tracked, no secrets — just the
// per-language Chirp 3: HD voice name, user-editable to swap in any of
// the 30 available voice personalities). Authentication is a separate
// concern; see GoogleServiceAccount/GoogleOAuthTokenProvider.
struct GoogleTtsConfig {
    // "google" or "sapi", from tts_config.json's top-level "provider".
    // Lives here (not just read ad hoc by TextToSpeechFactory) so the
    // Settings window has one struct representing the whole file to load,
    // edit, and save back.
    std::string provider = "sapi";

    // Keyed by audio::ToString(Language), e.g. "Korean" -> "ko-KR-Chirp3-HD-Kore".
    std::unordered_map<std::string, std::string> voicesByLanguage;

    // 1-100, from tts_config.json's top-level "volume_percent" (default
    // 100 = unchanged). Applies to both TTS providers: converted to
    // Cloud TTS's audioConfig.volumeGainDb for Google, passed straight to
    // ISpVoice::SetVolume for SAPI.
    int volumePercent = 100;

    static GoogleTtsConfig Load();
    void Save() const;
};

} // namespace sveta::audio
