#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>

namespace sveta::context {

// Project plan section 15 (Desktop Awareness): which application/window is
// currently active. Event-driven via SetWinEventHook rather than polling,
// per section 57's "Idle CPU/GPU 사용량 최소화" quality bar.
class ActiveWindowTracker {
public:
    struct WindowInfo {
        std::wstring title;
        std::wstring processName; // e.g. "devenv.exe", empty if unavailable
        HWND hwnd = nullptr;
    };
    using ChangeCallback = std::function<void(const WindowInfo&)>;

    // onChange fires on the thread that called Create (must have a message
    // loop — WinEventHook callbacks are dispatched through it).
    static std::unique_ptr<ActiveWindowTracker> Create(ChangeCallback onChange);
    ~ActiveWindowTracker();

    ActiveWindowTracker(const ActiveWindowTracker&) = delete;
    ActiveWindowTracker& operator=(const ActiveWindowTracker&) = delete;

    static WindowInfo GetCurrentWindowInfo();

private:
    ActiveWindowTracker(HWINEVENTHOOK hook, ChangeCallback onChange);

    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD threadId, DWORD time);

    HWINEVENTHOOK hook_;
    ChangeCallback onChange_;

    // SetWinEventHook's callback is a plain function pointer with no
    // user-data slot, and this app only ever needs one tracker instance.
    static ActiveWindowTracker* instance_;
};

} // namespace sveta::context
