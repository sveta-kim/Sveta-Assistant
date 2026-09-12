#pragma once

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "context/ActiveWindowTracker.h"
#include "context/GameDetector.h"
#include "context/PrivacyConfig.h"
#include "proactive/ErrorRepeatDetector.h"

namespace sveta::context {

struct ContextSnapshot {
    std::wstring processName;
    std::wstring windowTitle;
    std::wstring uiText; // shallow UI Automation summary; may be empty
    int generation = 0;  // internal: discards UI-text results for a window
                          // that's no longer the active one by the time
                          // the background read finishes
};

// Orchestrates Desktop Awareness (project plan sections 15-17): tracks the
// active window and, off the UI thread, pulls a shallow UI Automation
// text summary for it. Respects PrivacyConfig's on/off toggle and
// excluded-process list (section 51).
class ContextEngine {
public:
    // notifyWindow/notifyMessage: how the background UI Automation read
    // hands its result back to the UI thread, via PostMessage — the same
    // cross-thread pattern the AI/TTS code already uses.
    static std::unique_ptr<ContextEngine> Create(HWND notifyWindow, UINT notifyMessage);
    ~ContextEngine();

    ContextEngine(const ContextEngine&) = delete;
    ContextEngine& operator=(const ContextEngine&) = delete;

    bool IsEnabled() const { return privacy_.screenAwarenessEnabled; }

    // Best-known guess, updated on every active-window change, for whether
    // the user is currently in a game (see GameDetector). Independent of
    // the privacy toggle above — it drives the character's own behavior,
    // not what's sent to the AI, and reveals nothing beyond "you're in a
    // game" (no title/UI text).
    bool IsGaming() const { return privacy_.gameDetectionEnabled && isGaming_; }

    // Live updates from the Settings window (see window/SettingsWindow.h)
    // -- applied without recreating the engine, so the Steam/Epic library
    // scan and error-repeat history aren't thrown away just to flip a
    // toggle.
    void SetGameDetectionEnabled(bool enabled) { privacy_.gameDetectionEnabled = enabled; }
    void SetProactiveSpeechEnabled(bool enabled) { privacy_.proactiveSpeechEnabled = enabled; }
    void SetMemoryEnabled(bool enabled) { privacy_.memoryEnabled = enabled; }

    // Current foreground process's exe filename, for memory::MemoryEngine's
    // per-process active-time tally (Phase 8) -- tracked unconditionally
    // (like isGaming_ above) so it doesn't depend on the screen-awareness
    // toggle, but gated on its own memoryEnabled toggle and the excluded-
    // process list, same privacy posture as BuildContextLine. Empty when
    // memory is disabled or the process is excluded.
    std::wstring CurrentProcessNameForMemory() const;

    // Call when notifyMessage arrives at notifyWindow; reclaims and
    // applies the background thread's result.
    void OnSnapshotMessage(LPARAM lParam);

    // One-shot: non-empty exactly once, right after project plan section
    // 18's "동일 오류 반복" heuristic fires (see proactive::ErrorRepeatDetector),
    // as a ready-to-use situation description for a proactive AI system
    // message. Clears itself so MainWindow's tick won't re-trigger on the
    // next poll. Empty (and this event never fires) while screen awareness
    // is off or the active process is excluded, same as BuildContextLine.
    std::optional<std::wstring> ConsumeSameErrorRepeatedEvent();

    // Current best-known context as one short line for the AI system
    // prompt, e.g. "(사용자는 지금 devenv.exe 창(...)을 보고 있다)".
    // Empty if disabled, nothing tracked yet, or the active app is excluded.
    std::wstring BuildContextLine() const;

private:
    ContextEngine(std::unique_ptr<ActiveWindowTracker> tracker, HWND notifyWindow, UINT notifyMessage, PrivacyConfig privacy);

    void OnActiveWindowChanged(const ActiveWindowTracker::WindowInfo& info);
    bool IsExcluded(const std::wstring& processName) const;

    std::unique_ptr<ActiveWindowTracker> tracker_;
    HWND notifyWindow_;
    UINT notifyMessage_;
    PrivacyConfig privacy_;
    GameDetector gameDetector_;
    proactive::ErrorRepeatDetector errorRepeatDetector_;

    ContextSnapshot current_;
    bool hasSnapshot_ = false;
    int currentGeneration_ = 0;
    bool isGaming_ = false;
    bool pendingSameErrorRepeatedEvent_ = false;
    // Tracked unconditionally (independent of screenAwarenessEnabled), same
    // as isGaming_ -- see CurrentProcessNameForMemory().
    std::wstring currentProcessName_;
};

} // namespace sveta::context
