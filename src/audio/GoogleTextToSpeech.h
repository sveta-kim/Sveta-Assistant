#pragma once

#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "audio/GoogleOAuthTokenProvider.h"
#include "audio/GoogleServiceAccount.h"
#include "audio/GoogleTtsConfig.h"
#include "audio/ITextToSpeech.h"

namespace sveta::audio {

// Speaks AI replies via Google Cloud Text-to-Speech's Chirp 3: HD voices
// (see README) instead of the local SAPI voices TextToSpeech uses.
// Authenticates via a service account (GoogleOAuthTokenProvider), not a
// static API key -- this project's Google Cloud project has API key
// creation disabled by org policy.
//
// Unlike SAPI there's no OS-level async speech engine to lean on: Speak()
// does its own WinHTTP fetch on a background thread, then plays the
// result with PlaySoundW(SND_MEMORY). Google's LINEAR16 responses always
// include a WAV header, which conveniently gives an *exact* playback
// duration (computed from the header, not guessed) — so start/end
// notifications are simulated: "started" fires as soon as playback
// begins, "ended" fires after Sleep()ing that exact duration on the same
// background thread, rather than a real completion callback (PlaySoundW
// doesn't offer one).
class GoogleTextToSpeech : public ITextToSpeech {
public:
    // nullptr if the service account isn't usable (no key file configured)
    // — callers should fall back to local SAPI voices in that case.
    static std::unique_ptr<GoogleTextToSpeech> Create(
        HWND notifyWindow, UINT notifyMessage, GoogleTtsConfig config, GoogleServiceAccount account);
    ~GoogleTextToSpeech() override;

    GoogleTextToSpeech(const GoogleTextToSpeech&) = delete;
    GoogleTextToSpeech& operator=(const GoogleTextToSpeech&) = delete;

    void Speak(const std::wstring& text) override;
    void Stop() override;
    EventResult PumpEvents() override;

private:
    GoogleTextToSpeech(HWND notifyWindow, UINT notifyMessage, GoogleTtsConfig config, GoogleServiceAccount account);

    void SetPending(bool started, bool ended);

    HWND notifyWindow_;
    UINT notifyMessage_;
    GoogleTtsConfig config_;
    GoogleOAuthTokenProvider tokenProvider_;

    // Bumped on every Speak()/Stop() so a background thread whose fetch or
    // sleep finishes after being superseded (a newer message, a manual
    // stop) knows to discard its own completion instead of posting a
    // stale "started"/"ended" — same generation-counter idiom
    // ContextEngine uses for its own background UI Automation reads.
    std::atomic<int> generation_{0};

    std::mutex pendingMutex_;
    bool pendingStarted_ = false;
    bool pendingEnded_ = false;
};

} // namespace sveta::audio
