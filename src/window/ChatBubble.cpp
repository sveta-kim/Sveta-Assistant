#include "window/ChatBubble.h"

#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <thread>

namespace sveta::window {

namespace {
constexpr wchar_t kBubbleClassName[] = L"SvetaAssistantChatBubble";
constexpr wchar_t kFontFamilyName[] = L"Segoe UI";
constexpr int kBubbleWidth = 300;
constexpr int kInputHeight = 46;
constexpr int kBodyMinHeight = 46;
constexpr int kBodyMaxHeight = 200;
constexpr int kTailHeight = 12;
constexpr int kTailWidth = 22;
constexpr int kCornerRadius = 16;
constexpr int kPadding = 14;
constexpr int kGapAboveAnchor = 8;
constexpr UINT_PTR kAutoDismissTimerId = 1;

const Gdiplus::Color kFillColor(255, 255, 255, 255);
const Gdiplus::Color kBorderColor(255, 226, 226, 232);
const Gdiplus::Color kShadowColor(50, 20, 20, 30);
const Gdiplus::Color kTextColor(255, 40, 40, 44);

// GDI+ needs exactly one Startup/Shutdown pair for the process. Scoped to
// this file since ChatBubble is the only GDI+ user; a function-local
// static is initialized once, thread-safely, on first Create().
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

// GDI+'s font subsystem does slow, one-time lazy initialization on first
// use (observed: ~5s the first time a FontFamily/Font is constructed in
// this environment, ~4ms every time after). Doing that throwaway work on
// a background thread right after startup means the user's first chat
// message doesn't pay for it.
void WarmUpFontSubsystemAsync() {
    std::thread([]() {
        Gdiplus::FontFamily fontFamily(kFontFamilyName);
        Gdiplus::Font font(&fontFamily, 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        (void)font;
    }).detach();
}

// Builds the rounded-rectangle-with-tail outline used for both the shadow
// and the bubble body. Takes an out-param because Gdiplus::GraphicsPath
// hides its copy constructor, so it can't be returned by value.
void BuildBubblePath(Gdiplus::GraphicsPath& path, int width, int bodyHeight, int tailCenterX) {
    const int r = kCornerRadius;
    const int d = r * 2;
    const int halfTail = kTailWidth / 2;
    const int tailX = std::clamp(tailCenterX, r + halfTail + 2, width - r - halfTail - 2);

    path.AddArc(0, 0, d, d, 180, 90);                                        // top-left
    path.AddLine(r, 0, width - r, 0);                                        // top edge
    path.AddArc(width - d, 0, d, d, 270, 90);                                // top-right
    path.AddLine(width, r, width, bodyHeight - r);                          // right edge
    path.AddArc(width - d, bodyHeight - d, d, d, 0, 90);                    // bottom-right
    path.AddLine(width - r, bodyHeight, tailX + halfTail, bodyHeight);      // bottom edge (right)
    path.AddLine(tailX + halfTail, bodyHeight, tailX, bodyHeight + kTailHeight); // tail down
    path.AddLine(tailX, bodyHeight + kTailHeight, tailX - halfTail, bodyHeight); // tail up
    path.AddLine(tailX - halfTail, bodyHeight, r, bodyHeight);              // bottom edge (left)
    path.AddArc(0, bodyHeight - d, d, d, 90, 90);                           // bottom-left
    path.CloseFigure();
}

int MeasureBodyHeight(HWND hwnd, const std::wstring& text) {
    if (text.empty()) {
        return kBodyMinHeight;
    }
    const HDC dc = GetDC(hwnd);
    Gdiplus::Graphics graphics(dc);
    Gdiplus::FontFamily fontFamily(kFontFamilyName);
    Gdiplus::Font font(&fontFamily, 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::RectF boundRect(0, 0, static_cast<float>(kBubbleWidth - kPadding * 2), 10000.0f);
    Gdiplus::RectF resultRect;
    graphics.MeasureString(text.c_str(), -1, &font, boundRect, &resultRect);
    ReleaseDC(hwnd, dc);
    return static_cast<int>(resultRect.Height) + kPadding * 2;
}

} // namespace

std::unique_ptr<ChatBubble> ChatBubble::Create(HINSTANCE instance) {
    EnsureGdiplusStarted();
    WarmUpFontSubsystemAsync();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &ChatBubble::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    // Used only in (non-layered) input mode; ignored while layered.
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    wc.lpszClassName = kBubbleClassName;
    // Ignore failure: benign if already registered (Create is only called
    // once, but this keeps a second call from crashing).
    RegisterClassExW(&wc);

    const HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kBubbleClassName, L"", WS_POPUP,
        0, 0, kBubbleWidth, kInputHeight,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        return nullptr;
    }

    const HFONT editFont = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, kFontFamilyName);
    const HBRUSH editBackgroundBrush = CreateSolidBrush(RGB(255, 255, 255));

    const HWND edit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL,
        kPadding, kPadding / 2, kBubbleWidth - kPadding * 2, kInputHeight - kPadding,
        hwnd, nullptr, instance, nullptr);
    if (!edit) {
        DestroyWindow(hwnd);
        DeleteObject(editFont);
        DeleteObject(editBackgroundBrush);
        return nullptr;
    }
    SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(editFont), TRUE);

    auto bubble = std::unique_ptr<ChatBubble>(new ChatBubble(hwnd, edit, editFont, editBackgroundBrush));
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(bubble.get()));
    SetWindowSubclass(edit, &ChatBubble::EditSubclassProc, 0, reinterpret_cast<DWORD_PTR>(bubble.get()));

    return bubble;
}

ChatBubble::ChatBubble(HWND hwnd, HWND edit, HFONT editFont, HBRUSH editBackgroundBrush)
    : hwnd_(hwnd), edit_(edit), editFont_(editFont), editBackgroundBrush_(editBackgroundBrush) {}

ChatBubble::~ChatBubble() {
    if (hwnd_) {
        DestroyWindow(hwnd_); // also destroys the child edit control
    }
    if (editFont_) {
        DeleteObject(editFont_);
    }
    if (editBackgroundBrush_) {
        DeleteObject(editBackgroundBrush_);
    }
}

void ChatBubble::EnterInputMode(POINT anchorTop) {
    // UpdateLayeredWindow-driven layered windows don't composite normal
    // child controls, so input mode must not be layered.
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED) {
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    }

    SetWindowTextW(edit_, L"");
    SendMessageW(edit_, EM_SETREADONLY, FALSE, 0);
    ShowWindow(edit_, SW_SHOW);

    const HRGN region = CreateRoundRectRgn(0, 0, kBubbleWidth + 1, kInputHeight + 1, kCornerRadius * 2, kCornerRadius * 2);
    SetWindowRgn(hwnd_, region, FALSE); // ownership of region transfers to the window

    const int x = anchorTop.x - kBubbleWidth / 2;
    const int y = anchorTop.y - kInputHeight - kGapAboveAnchor;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, kBubbleWidth, kInputHeight, SWP_NOACTIVATE);
    MoveWindow(edit_, kPadding, kPadding / 2, kBubbleWidth - kPadding * 2, kInputHeight - kPadding, TRUE);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void ChatBubble::EnterStaticMode(POINT anchorTop, const std::wstring& text) {
    ShowWindow(edit_, SW_HIDE);

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED)) {
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }
    SetWindowRgn(hwnd_, nullptr, FALSE); // layered mode uses per-pixel alpha, not a region

    const int bodyHeight = std::clamp(MeasureBodyHeight(hwnd_, text), kBodyMinHeight, kBodyMaxHeight);
    const int totalHeight = bodyHeight + kTailHeight;
    const POINT screenPos{anchorTop.x - kBubbleWidth / 2, anchorTop.y - totalHeight - kGapAboveAnchor};

    PaintStaticBubble(kBubbleWidth, totalHeight, screenPos, text);
}

void ChatBubble::PaintStaticBubble(int width, int height, POINT screenPos, const std::wstring& text) {
    const int bodyHeight = height - kTailHeight;

    Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    const int tailCenterX = width / 2;

    // Soft shadow: same silhouette, offset down, low alpha.
    {
        Gdiplus::GraphicsPath shadowPath;
        BuildBubblePath(shadowPath, width, bodyHeight, tailCenterX);
        Gdiplus::Matrix offset(1, 0, 0, 1, 0, 3);
        shadowPath.Transform(&offset);
        Gdiplus::SolidBrush shadowBrush(kShadowColor);
        graphics.FillPath(&shadowBrush, &shadowPath);
    }

    // Bubble body + border.
    {
        Gdiplus::GraphicsPath path;
        BuildBubblePath(path, width, bodyHeight, tailCenterX);
        Gdiplus::SolidBrush fillBrush(kFillColor);
        graphics.FillPath(&fillBrush, &path);
        Gdiplus::Pen borderPen(kBorderColor, 1.25f);
        graphics.DrawPath(&borderPen, &path);
    }

    // Text.
    if (!text.empty()) {
        Gdiplus::FontFamily fontFamily(kFontFamilyName);
        Gdiplus::Font font(&fontFamily, 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBrush(kTextColor);
        Gdiplus::RectF textRect(
            static_cast<float>(kPadding), static_cast<float>(kPadding * 0.6f),
            static_cast<float>(width - kPadding * 2), static_cast<float>(bodyHeight - kPadding));
        graphics.DrawString(text.c_str(), -1, &font, textRect, nullptr, &textBrush);
    }

    Gdiplus::BitmapData bitmapData{};
    Gdiplus::Rect lockRect(0, 0, width, height);
    bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bitmapData);

    const HDC screenDc = GetDC(nullptr);
    const HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    const HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        // GDI+'s PixelFormat32bppPARGB stride can exceed width*4 (DWORD
        // alignment), so copy row by row rather than a single memcpy.
        auto* dst = static_cast<uint8_t*>(bits);
        const auto* src = static_cast<const uint8_t*>(bitmapData.Scan0);
        for (int y = 0; y < height; ++y) {
            memcpy(dst + y * width * 4, src + y * bitmapData.Stride, static_cast<size_t>(width) * 4);
        }

        const HGDIOBJ oldBitmap = SelectObject(memDc, dib);
        POINT srcPoint{0, 0};
        POINT dstPoint = screenPos;
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(hwnd_, screenDc, &dstPoint, &size, memDc, &srcPoint, 0, &blend, ULW_ALPHA);

        SelectObject(memDc, oldBitmap);
        DeleteObject(dib);
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    bitmap.UnlockBits(&bitmapData);
}

void ChatBubble::OpenForInput(POINT anchorTop, SubmitCallback onSubmit, DismissCallback onDismiss) {
    onSubmit_ = std::move(onSubmit);
    onDismiss_ = std::move(onDismiss);
    KillTimer(hwnd_, kAutoDismissTimerId);

    EnterInputMode(anchorTop);

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    SetForegroundWindow(hwnd_);
    SetFocus(edit_);
}

void ChatBubble::ShowThinking(POINT anchorTop) {
    onSubmit_ = nullptr;
    onDismiss_ = nullptr;
    KillTimer(hwnd_, kAutoDismissTimerId);

    EnterStaticMode(anchorTop, L"...");
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void ChatBubble::ShowResponse(POINT anchorTop, const std::wstring& text, DismissCallback onAutoDismiss) {
    onSubmit_ = nullptr;
    onDismiss_ = std::move(onAutoDismiss);

    EnterStaticMode(anchorTop, text);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    const int durationMs = std::clamp(static_cast<int>(text.size()) * 60 + 2000, 3000, 15000);
    SetTimer(hwnd_, kAutoDismissTimerId, static_cast<UINT>(durationMs), nullptr);
}

void ChatBubble::Hide() {
    KillTimer(hwnd_, kAutoDismissTimerId);
    ShowWindow(hwnd_, SW_HIDE);
}

bool ChatBubble::IsVisible() const {
    return IsWindowVisible(hwnd_);
}

void ChatBubble::HandleEditReturn() {
    const int len = GetWindowTextLengthW(edit_);
    if (len <= 0) {
        return;
    }
    std::wstring text(static_cast<size_t>(len), L'\0');
    GetWindowTextW(edit_, text.data(), len + 1);
    if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        return; // whitespace-only: ignore
    }

    if (onSubmit_) {
        onSubmit_(text); // caller drives the next mode (ShowThinking, etc.)
    }
}

void ChatBubble::HandleEditEscape() {
    const DismissCallback callback = std::move(onDismiss_);
    Hide();
    if (callback) {
        callback();
    }
}

LRESULT CALLBACK ChatBubble::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<ChatBubble*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT ChatBubble::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CTLCOLOREDIT: {
            const HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, RGB(40, 40, 44));
            SetBkColor(hdc, RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBackgroundBrush_);
        }
        case WM_TIMER:
            if (wParam == kAutoDismissTimerId) {
                const DismissCallback callback = std::move(onDismiss_);
                Hide();
                if (callback) {
                    callback();
                }
            }
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
    }
}

LRESULT CALLBACK ChatBubble::EditSubclassProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR /*subclassId*/, DWORD_PTR refData) {
    auto* self = reinterpret_cast<ChatBubble*>(refData);

    if (message == WM_KEYDOWN && wParam == VK_RETURN) {
        self->HandleEditReturn();
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) {
        self->HandleEditEscape();
        return 0;
    }
    if (message == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        return 0; // swallow so it doesn't beep or insert a newline
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

} // namespace sveta::window
