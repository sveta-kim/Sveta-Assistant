#include "audio/TextToSpeechFactory.h"

#include "audio/GoogleServiceAccount.h"
#include "audio/GoogleTextToSpeech.h"
#include "audio/GoogleTtsConfig.h"
#include "audio/TextToSpeech.h"
#include "core/Logger.h"

namespace sveta::audio {

std::unique_ptr<ITextToSpeech> CreateTextToSpeech(HWND notifyWindow, UINT notifyMessage) {
    // Loaded once regardless of provider: volume_percent applies to
    // whichever engine actually ends up speaking.
    const GoogleTtsConfig config = GoogleTtsConfig::Load();

    if (config.provider == "google") {
        if (auto google = GoogleTextToSpeech::Create(notifyWindow, notifyMessage, config, GoogleServiceAccount::Load())) {
            core::Logger::Info("TextToSpeech: using Google Cloud TTS (Chirp 3: HD)");
            return google;
        }
        core::Logger::Warn(
            "TextToSpeech: provider is \"google\" but config/google_service_account.json is missing or "
            "incomplete; falling back to local SAPI voices");
    }

    return TextToSpeech::Create(notifyWindow, notifyMessage, config.volumePercent);
}

} // namespace sveta::audio
