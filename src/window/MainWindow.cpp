#include "window/MainWindow.h"

#include "core/Logger.h"

namespace sveta::window {

namespace {
constexpr wchar_t kWindowClassName[] = L"SvetaAssistantWindowClass";
constexpr int kDefaultWidth = 300;
constexpr int kDefaultHeight = 300;
} // namespace

std::unique_ptr<MainWindow> MainWindow::Create(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &MainWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        core::Logger::Error("Failed to register window class");
        return nullptr;
    }

    // WS_POPUP: borderless. WS_EX_LAYERED: per-pixel transparency support.
    // WS_EX_TOPMOST: always on top. WS_EX_TOOLWINDOW: hide from taskbar/alt-tab.
    const HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kWindowClassName,
        L"Sveta Assistant",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        kDefaultWidth, kDefaultHeight,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) {
        core::Logger::Error("Failed to create main window");
        return nullptr;
    }

    // Placeholder transparency: full opacity for now. Per-pixel alpha (PNG
    // sprite) rendering lands with Phase 1 character rendering work.
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    auto window = std::unique_ptr<MainWindow>(new MainWindow(hwnd));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    core::Logger::Info("Main window created");
    return window;
}

MainWindow::MainWindow(HWND hwnd) : hwnd_(hwnd) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

MainWindow::~MainWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

int MainWindow::RunMessageLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            core::Logger::Info("Main window destroyed");
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

} // namespace sveta::window
