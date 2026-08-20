#pragma once

#include <windows.h>
#include <wrl/client.h>

#include <memory>
#include <string>

struct ISpVoice; // avoid pulling <sapi.h> into every includer

namespace sveta::audio {

// Wraps SAPI (ISpVoice) for fire-and-forget speech with start/end
// notifications delivered as a window message — SAPI's own async speech
// runs on its own thread internally, so no thread of our own is needed.
// Voice choice is picked per-utterance based on whether the text looks
// like it contains Hangul, since per-character Voice Profiles (project
// plan sections 21, 25) don't exist yet.
class TextToSpeech {
public:
    static std::unique_ptr<TextToSpeech> Create(HWND notifyWindow, UINT notifyMessage);
    ~TextToSpeech();

    TextToSpeech(const TextToSpeech&) = delete;
    TextToSpeech& operator=(const TextToSpeech&) = delete;

    void Speak(const std::wstring& text);
    void Stop();

    struct EventResult {
        bool started = false;
        bool ended = false;
    };
    // Call when notifyMessage arrives at the notify window; drains SAPI's
    // event queue and reports which of start/end occurred since the last call.
    EventResult PumpEvents();

private:
    explicit TextToSpeech(Microsoft::WRL::ComPtr<ISpVoice> voice);

    Microsoft::WRL::ComPtr<ISpVoice> voice_;
};

} // namespace sveta::audio
