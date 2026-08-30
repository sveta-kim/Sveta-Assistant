#pragma once

#include <string>

namespace sveta::audio {

// Common surface both TextToSpeech (local SAPI voices) and
// GoogleTextToSpeech (Google Cloud TTS, Chirp 3: HD) implement, so
// MainWindow doesn't need to know which backend is actually speaking —
// see audio/TextToSpeechFactory.h for how the choice is made.
class ITextToSpeech {
public:
    virtual ~ITextToSpeech() = default;

    virtual void Speak(const std::wstring& text) = 0;
    virtual void Stop() = 0;

    struct EventResult {
        bool started = false;
        bool ended = false;
    };
    // Call when the notify message (passed to whichever factory function
    // created this instance) arrives at the notify window; reports which
    // of start/end occurred since the last call.
    virtual EventResult PumpEvents() = 0;
};

} // namespace sveta::audio
