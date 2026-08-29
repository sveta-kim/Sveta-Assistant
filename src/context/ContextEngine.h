#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "context/ActiveWindowTracker.h"
#include "context/PrivacyConfig.h"

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

    // Call when notifyMessage arrives at notifyWindow; reclaims and
    // applies the background thread's result.
    void OnSnapshotMessage(LPARAM lParam);

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

    ContextSnapshot current_;
    bool hasSnapshot_ = false;
    int currentGeneration_ = 0;
};

} // namespace sveta::context
