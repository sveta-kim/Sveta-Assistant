#include "items/ItemWindow.h"

#include <filesystem>
#include <format>

#include "core/Logger.h"
#include "window/WindowPosition.h"

namespace sveta::items {

namespace {
constexpr wchar_t kItemWindowClassName[] = L"SvetaAssistantItemWindow";
constexpr wchar_t kMugPositionFileName[] = L"coffee_mug_position.txt";
// Native art is already sized for on-screen display; passed anyway for
// consistency with every other Sprite::LoadFromFile call site and as a
// defensive cap if the art is ever swapped for something larger.
constexpr uint32_t kMaxMugDimension = 96;
constexpr int kSpawnOffsetX = 24;
constexpr int kScreenMargin = 40;

std::filesystem::path MugSpritePath() {
    return std::filesystem::path(SVETA_CONTENT_DIR) / L"items" / L"coffee_mug.png";
}

// Fallback default if there's no valid saved position: bottom-left corner
// of the primary monitor, mirroring MainWindow's DefaultPosition (which
// uses the bottom-right for the character) so the two don't default on top
// of each other on first run.
POINT DefaultPosition(int height) {
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    return POINT{kScreenMargin, screenHeight - height - kScreenMargin};
}

// Same rationale as MainWindow's IsPositionOnAnyMonitor: reject a saved (or
// computed) position from a monitor arrangement that no longer exists.
bool IsPositionOnAnyMonitor(POINT position, int width, int height) {
    RECT rect{position.x, position.y, position.x + width, position.y + height};
    return MonitorFromRect(&rect, MONITOR_DEFAULTTONULL) != nullptr;
}

POINT SpawnPositionNextToCharacter(HWND characterHwnd, int height) {
    RECT characterRect{};
    GetWindowRect(characterHwnd, &characterRect);
    return POINT{
        characterRect.right + kSpawnOffsetX,
        characterRect.top + (characterRect.bottom - characterRect.top) / 2 - height / 2,
    };
}

} // namespace

std::unique_ptr<ItemWindow> ItemWindow::Create(HINSTANCE instance, HWND characterHwnd, OfferCallback onOffered) {
    auto sprite = rendering::Sprite::LoadFromFile(MugSpritePath(), kMaxMugDimension);
    if (!sprite) {
        core::Logger::Warn("No coffee mug sprite loaded; skipping item window");
        return nullptr;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &ItemWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kItemWindowClassName;
    // Ignore failure: benign if already registered.
    RegisterClassExW(&wc);

    const int width = static_cast<int>(sprite->Width());
    const int height = static_cast<int>(sprite->Height());

    POINT position = SpawnPositionNextToCharacter(characterHwnd, height);
    if (!IsPositionOnAnyMonitor(position, width, height)) {
        position = DefaultPosition(height);
    }
    if (const auto savedPosition = window::LoadWindowPosition(kMugPositionFileName);
        savedPosition && IsPositionOnAnyMonitor(*savedPosition, width, height)) {
        position = *savedPosition;
    }

    const HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kItemWindowClassName, L"Coffee Mug", WS_POPUP,
        position.x, position.y, width, height,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        core::Logger::Error("Failed to create coffee mug item window");
        return nullptr;
    }

    auto item = std::unique_ptr<ItemWindow>(
        new ItemWindow(hwnd, characterHwnd, std::move(*sprite), std::move(onOffered)));
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(item.get()));
    item->ApplySpriteToWindow();
    return item;
}

ItemWindow::ItemWindow(HWND hwnd, HWND characterHwnd, rendering::Sprite sprite, OfferCallback onOffered)
    : hwnd_(hwnd), characterHwnd_(characterHwnd), sprite_(std::move(sprite)), onOffered_(std::move(onOffered)) {}

ItemWindow::~ItemWindow() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

void ItemWindow::Show() {
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE); // never steal focus from whatever the user's doing
}

void ItemWindow::ApplySpriteToWindow() {
    // Duplicated blit routine (a 3rd copy in the codebase alongside
    // MainWindow::ApplyPixelsToWindow / ChatBubble::PaintStaticBubble) --
    // small enough that a shared helper isn't worth the coupling.
    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);

    const HDC screenDc = GetDC(nullptr);
    const HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(sprite_.Width());
    bmi.bmiHeader.biHeight = -static_cast<LONG>(sprite_.Height()); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    const HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        const auto& pixels = sprite_.PremultipliedBgra();
        memcpy(bits, pixels.data(), pixels.size());

        const HGDIOBJ oldBitmap = SelectObject(memDc, dib);

        POINT srcPoint{0, 0};
        POINT dstPoint{windowRect.left, windowRect.top};
        SIZE size{static_cast<LONG>(sprite_.Width()), static_cast<LONG>(sprite_.Height())};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

        if (!UpdateLayeredWindow(hwnd_, screenDc, &dstPoint, &size, memDc, &srcPoint, 0, &blend, ULW_ALPHA)) {
            core::Logger::Error(std::format("UpdateLayeredWindow failed for item window (error={})", GetLastError()));
        }

        SelectObject(memDc, oldBitmap);
        DeleteObject(dib);
    } else {
        core::Logger::Error("Failed to create DIB section for item sprite");
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

void ItemWindow::SaveCurrentPosition() const {
    RECT rect{};
    if (GetWindowRect(hwnd_, &rect)) {
        window::SaveWindowPosition(POINT{rect.left, rect.top}, kMugPositionFileName);
    }
}

void ItemWindow::HandleDragEnd() {
    RECT itemRect{};
    RECT characterRect{};
    GetWindowRect(hwnd_, &itemRect);
    GetWindowRect(characterHwnd_, &characterRect);

    RECT intersection{};
    const bool overlapped = IntersectRect(&intersection, &itemRect, &characterRect) != 0;

    SaveCurrentPosition();

    if (overlapped) {
        core::Logger::Info("Coffee mug offered to Sveta");
        if (onOffered_) {
            onOffered_();
        }
    }
    // No snap-back on offer: the mug simply stays wherever it was dropped.
}

LRESULT CALLBACK ItemWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<ItemWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT ItemWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
            // Same borderless-drag trick as MainWindow's WM_LBUTTONDOWN.
            ReleaseCapture();
            SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        case WM_EXITSIZEMOVE:
            HandleDragEnd();
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

} // namespace sveta::items
