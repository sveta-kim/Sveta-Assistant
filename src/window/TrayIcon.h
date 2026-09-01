#pragma once

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <memory>
#include <string>

namespace sveta::window {

// Wraps a single Shell_NotifyIcon system tray entry. The icon bitmap is
// loaded from a PNG file (not a compiled .ico resource) via GDI+, so the
// character's own face art (content/face.png) can be reused directly
// instead of maintaining a separate .ico asset.
class TrayIcon {
public:
    // iconPngPath: an RGBA PNG (ideally with a transparent background),
    // resized down to tray-icon size at load time.
    // callbackMessage: posted to notifyWindow on tray icon mouse events;
    // wParam/lParam follow the classic Shell_NotifyIcon convention (lParam
    // carries the mouse message, e.g. WM_RBUTTONUP/WM_CONTEXTMENU).
    static std::unique_ptr<TrayIcon> Create(
        HWND notifyWindow, UINT callbackMessage, const std::filesystem::path& iconPngPath,
        const std::wstring& tooltip);
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    void SetTooltip(const std::wstring& tooltip);

    // Explorer forgets every tray icon when it restarts, and announces
    // that with a registered "TaskbarCreated" message; call this when it
    // arrives at notifyWindow to get the icon back. The message ID is
    // assigned at runtime (RegisterWindowMessageW), so it can't be a
    // switch-case constant -- compare message == TaskbarCreatedMessage()
    // in a default: branch instead.
    void Readd();
    static UINT TaskbarCreatedMessage();

private:
    TrayIcon(HWND notifyWindow, UINT callbackMessage, HICON icon, bool ownsIcon, std::wstring tooltip);

    void FillData(NOTIFYICONDATAW& data) const;

    HWND notifyWindow_;
    UINT callbackMessage_;
    HICON icon_;
    // False when icon_ is the shared system fallback (LoadIconW(nullptr,
    // IDI_APPLICATION)) rather than one we created via GDI+ -- DestroyIcon
    // must not be called on a shared icon.
    bool ownsIcon_;
    std::wstring tooltip_;
};

} // namespace sveta::window
