#include "window/TrayIcon.h"

#include <objidl.h> // must precede gdiplus.h: GDI+ headers need IStream from here
#include <gdiplus.h>

#include "core/Logger.h"

namespace sveta::window {

namespace {

constexpr UINT kTrayIconId = 1;
// Windows scales this down to whatever the actual tray slot size is for
// the current DPI; 32 gives it enough source detail to downscale cleanly.
constexpr int kIconSize = 32;

// Same self-contained GDI+ Startup/Shutdown pattern as ChatBubble.cpp --
// each module that touches GDI+ pairs its own token via a function-local
// static, which GDI+ supports across multiple call sites in one process.
class GdiplusScope {
public:
    GdiplusScope() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&token_, &input, nullptr);
    }
    ~GdiplusScope() { Gdiplus::GdiplusShutdown(token_); }
    GdiplusScope(const GdiplusScope&) = delete;
    GdiplusScope& operator=(const GdiplusScope&) = delete;

private:
    ULONG_PTR token_ = 0;
};

void EnsureGdiplusStarted() {
    static GdiplusScope scope;
    (void)scope;
}

HICON LoadIconFromPng(const std::filesystem::path& path) {
    EnsureGdiplusStarted();

    Gdiplus::Bitmap source(path.wstring().c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) {
        core::Logger::Warn("TrayIcon: failed to load icon PNG at " + path.string());
        return nullptr;
    }

    Gdiplus::Bitmap resized(kIconSize, kIconSize, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&resized);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    graphics.DrawImage(&source, 0, 0, kIconSize, kIconSize);

    HICON icon = nullptr;
    resized.GetHICON(&icon); // GetHICON preserves alpha for 32bpp PARGB bitmaps
    return icon;
}

} // namespace

std::unique_ptr<TrayIcon> TrayIcon::Create(
    HWND notifyWindow, UINT callbackMessage, const std::filesystem::path& iconPngPath, const std::wstring& tooltip) {
    HICON icon = LoadIconFromPng(iconPngPath);
    bool ownsIcon = icon != nullptr;
    if (!icon) {
        icon = LoadIconW(nullptr, IDI_APPLICATION); // shared system icon; DestroyIcon must not touch this
    }

    auto trayIcon = std::unique_ptr<TrayIcon>(new TrayIcon(notifyWindow, callbackMessage, icon, ownsIcon, tooltip));

    NOTIFYICONDATAW data{};
    trayIcon->FillData(data);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        core::Logger::Warn("TrayIcon: Shell_NotifyIcon(NIM_ADD) failed");
    }

    return trayIcon;
}

TrayIcon::TrayIcon(HWND notifyWindow, UINT callbackMessage, HICON icon, bool ownsIcon, std::wstring tooltip)
    : notifyWindow_(notifyWindow),
      callbackMessage_(callbackMessage),
      icon_(icon),
      ownsIcon_(ownsIcon),
      tooltip_(std::move(tooltip)) {}

TrayIcon::~TrayIcon() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = notifyWindow_;
    data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    if (icon_ && ownsIcon_) {
        DestroyIcon(icon_);
    }
}

void TrayIcon::FillData(NOTIFYICONDATAW& data) const {
    data.cbSize = sizeof(data);
    data.hWnd = notifyWindow_;
    data.uID = kTrayIconId;
    data.uCallbackMessage = callbackMessage_;
    data.hIcon = icon_;
    wcsncpy_s(data.szTip, tooltip_.c_str(), _TRUNCATE);
}

void TrayIcon::SetTooltip(const std::wstring& tooltip) {
    tooltip_ = tooltip;
    NOTIFYICONDATAW data{};
    FillData(data);
    data.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::Readd() {
    NOTIFYICONDATAW data{};
    FillData(data);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_ADD, &data);
}

UINT TrayIcon::TaskbarCreatedMessage() {
    static const UINT message = RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

} // namespace sveta::window
