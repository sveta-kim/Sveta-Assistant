#include "window/MainWindow.h"

#include <windowsx.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <string>

#include "core/Logger.h"
#include "interaction/HeadHitbox.h"
#include "window/WindowPosition.h"

namespace sveta::window {

namespace {
constexpr wchar_t kWindowClassName[] = L"SvetaAssistantWindowClass";
constexpr int kFallbackSize = 300;
constexpr int kScreenMargin = 40;
// On-screen size cap for character art; source art (e.g. 1254x1254) is
// downscaled to fit, never upscaled.
constexpr uint32_t kMaxCharacterDimension = 240;

constexpr UINT_PTR kCharacterTickTimerId = 1;
constexpr UINT kCharacterTickIntervalMs = 1000;

// TODO(Phase 9 - Item System / Content Platform): replace with the real
// character package loader (character.json -> assets/). SVETA_CONTENT_DIR
// points at the repo's content/ directory for local development only.
std::filesystem::path SpritePathForEmotion(character::Emotion emotion) {
    const std::string fileName(character::SpriteFileName(emotion));
    return std::filesystem::path(SVETA_CONTENT_DIR) / L"characters" / L"sveta" / L"assets" / fileName;
}

POINT DefaultPosition(int width, int height) {
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    return POINT{
        screenWidth - width - kScreenMargin,
        screenHeight - height - kScreenMargin,
    };
}

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

    auto sprite = rendering::Sprite::LoadFromFile(SpritePathForEmotion(character::Emotion::Calm), kMaxCharacterDimension);
    if (!sprite) {
        core::Logger::Warn("No character sprite loaded; falling back to a blank placeholder window");
    }

    const int width = sprite ? static_cast<int>(sprite->Width()) : kFallbackSize;
    const int height = sprite ? static_cast<int>(sprite->Height()) : kFallbackSize;

    const POINT position = LoadWindowPosition().value_or(DefaultPosition(width, height));

    // WS_POPUP: borderless. WS_EX_LAYERED: per-pixel transparency support.
    // WS_EX_TOPMOST: always on top. WS_EX_TOOLWINDOW: hide from taskbar/alt-tab.
    const HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kWindowClassName,
        L"Sveta Assistant",
        WS_POPUP,
        position.x, position.y,
        width, height,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) {
        core::Logger::Error("Failed to create main window");
        return nullptr;
    }

    auto window = std::unique_ptr<MainWindow>(new MainWindow(hwnd, std::move(sprite)));
    window->ApplySpriteToWindow();

    ShowWindow(hwnd, SW_SHOW);
    SetTimer(hwnd, kCharacterTickTimerId, kCharacterTickIntervalMs, nullptr);

    core::Logger::Info("Main window created");
    return window;
}

MainWindow::MainWindow(HWND hwnd, std::optional<rendering::Sprite> sprite)
    : hwnd_(hwnd), sprite_(std::move(sprite)) {
    if (sprite_) {
        headHitbox_ = interaction::ComputeHeadHitbox(sprite_->Width(), sprite_->Height());
    }
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

MainWindow::~MainWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

void MainWindow::ApplySpriteToWindow() {
    if (!sprite_) {
        // No PNG available yet: fall back to a fully opaque rectangle so the
        // window is at least visible during early development.
        SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
        return;
    }

    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);

    const HDC screenDc = GetDC(nullptr);
    const HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(sprite_->Width());
    bmi.bmiHeader.biHeight = -static_cast<LONG>(sprite_->Height()); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    const HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        memcpy(bits, sprite_->PremultipliedBgra().data(), sprite_->PremultipliedBgra().size());

        const HGDIOBJ oldBitmap = SelectObject(memDc, dib);

        POINT srcPoint{0, 0};
        POINT dstPoint{windowRect.left, windowRect.top};
        SIZE size{static_cast<LONG>(sprite_->Width()), static_cast<LONG>(sprite_->Height())};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

        if (!UpdateLayeredWindow(hwnd_, screenDc, &dstPoint, &size, memDc, &srcPoint, 0, &blend, ULW_ALPHA)) {
            core::Logger::Error(std::format("UpdateLayeredWindow failed (error={})", GetLastError()));
        }

        SelectObject(memDc, oldBitmap);
        DeleteObject(dib);
    } else {
        core::Logger::Error("Failed to create DIB section for sprite");
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

void MainWindow::ReloadSpriteForEmotion(character::Emotion emotion) {
    auto sprite = rendering::Sprite::LoadFromFile(SpritePathForEmotion(emotion), kMaxCharacterDimension);
    if (!sprite) {
        core::Logger::Warn(std::format("No sprite for emotion {}; falling back to calm", character::ToString(emotion)));
        sprite = rendering::Sprite::LoadFromFile(SpritePathForEmotion(character::Emotion::Calm), kMaxCharacterDimension);
    }
    if (!sprite) {
        return; // keep whatever is currently displayed
    }

    sprite_ = std::move(sprite);
    headHitbox_ = interaction::ComputeHeadHitbox(sprite_->Width(), sprite_->Height());
    ApplySpriteToWindow();
}

void MainWindow::SyncSpriteToEmotion() {
    const character::Emotion emotion = characterState_.CurrentEmotion();
    if (emotion == lastAppliedEmotion_) {
        return;
    }
    lastAppliedEmotion_ = emotion;
    ReloadSpriteForEmotion(emotion);
}

void MainWindow::SaveCurrentPosition() {
    RECT rect{};
    if (GetWindowRect(hwnd_, &rect)) {
        SaveWindowPosition(POINT{rect.left, rect.top});
    }
}

void MainWindow::HandleMouseMove(LPARAM lParam) {
    const auto now = std::chrono::steady_clock::now();

    if (!isHovering_) {
        isHovering_ = true;
        core::Logger::Info("CharacterHovered");
        characterState_.OnHoverStart(now);

        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
    }

    const POINT localPos{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (PtInRect(&headHitbox_, localPos)) {
        if (pettingDetector_.OnCursorMove(localPos, now)) {
            core::Logger::Info("CharacterPetted");
            characterState_.OnPetted(now);
        }
    } else {
        pettingDetector_.Reset();
    }

    SyncSpriteToEmotion();
}

void MainWindow::HandleMouseLeave() {
    isHovering_ = false;
    pettingDetector_.Reset();
    characterState_.OnHoverEnd();
    core::Logger::Info("Character hover ended");
    SyncSpriteToEmotion();
}

void MainWindow::HandleTick() {
    characterState_.Tick(std::chrono::steady_clock::now(), isHovering_);
    SyncSpriteToEmotion();
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
        case WM_LBUTTONDOWN:
            // Borderless drag: let DefWindowProc's caption-move loop handle
            // it as if the user grabbed a title bar (there is none).
            ReleaseCapture();
            SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        case WM_MOUSEMOVE:
            HandleMouseMove(lParam);
            return 0;
        case WM_MOUSELEAVE:
            HandleMouseLeave();
            return 0;
        case WM_ENTERSIZEMOVE:
            // Fired by the caption-move loop the WM_LBUTTONDOWN trick enters.
            characterState_.OnDragStart(std::chrono::steady_clock::now());
            SyncSpriteToEmotion();
            return 0;
        case WM_EXITSIZEMOVE:
            characterState_.OnDragEnd(std::chrono::steady_clock::now(), isHovering_);
            SyncSpriteToEmotion();
            SaveCurrentPosition();
            return 0;
        case WM_TIMER:
            if (wParam == kCharacterTickTimerId) {
                HandleTick();
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kCharacterTickTimerId);
            SaveCurrentPosition();
            core::Logger::Info("Main window destroyed");
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

} // namespace sveta::window
