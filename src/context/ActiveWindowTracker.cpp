#include "context/ActiveWindowTracker.h"

#include <processthreadsapi.h>

#include <filesystem>

namespace sveta::context {

ActiveWindowTracker* ActiveWindowTracker::instance_ = nullptr;

ActiveWindowTracker::WindowInfo ActiveWindowTracker::GetCurrentWindowInfo() {
    WindowInfo info;
    const HWND fg = GetForegroundWindow();
    if (!fg) {
        return info;
    }
    info.hwnd = fg;

    wchar_t titleBuf[512]{};
    GetWindowTextW(fg, titleBuf, static_cast<int>(std::size(titleBuf)));
    info.title = titleBuf;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid != 0) {
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process) {
            wchar_t pathBuf[MAX_PATH]{};
            DWORD size = static_cast<DWORD>(std::size(pathBuf));
            if (QueryFullProcessImageNameW(process, 0, pathBuf, &size)) {
                info.processName = std::filesystem::path(pathBuf).filename().wstring();
            }
            CloseHandle(process);
        }
    }

    return info;
}

std::unique_ptr<ActiveWindowTracker> ActiveWindowTracker::Create(ChangeCallback onChange) {
    // Out-of-context hook: the OS dispatches it via a message posted to
    // this thread's queue, so no dedicated hook thread is needed as long
    // as this thread keeps pumping messages (MainWindow's message loop).
    const HWINEVENTHOOK hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, &ActiveWindowTracker::WinEventProc,
        0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hook) {
        return nullptr;
    }

    auto tracker = std::unique_ptr<ActiveWindowTracker>(new ActiveWindowTracker(hook, std::move(onChange)));
    instance_ = tracker.get();
    return tracker;
}

ActiveWindowTracker::ActiveWindowTracker(HWINEVENTHOOK hook, ChangeCallback onChange)
    : hook_(hook), onChange_(std::move(onChange)) {}

ActiveWindowTracker::~ActiveWindowTracker() {
    if (hook_) {
        UnhookWinEvent(hook_);
    }
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void CALLBACK ActiveWindowTracker::WinEventProc(
    HWINEVENTHOOK /*hook*/, DWORD event, HWND /*hwnd*/, LONG idObject, LONG /*idChild*/, DWORD /*threadId*/, DWORD /*time*/) {
    if (event != EVENT_SYSTEM_FOREGROUND || idObject != OBJID_WINDOW || !instance_ || !instance_->onChange_) {
        return;
    }
    instance_->onChange_(GetCurrentWindowInfo());
}

} // namespace sveta::context
