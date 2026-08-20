#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>

namespace sveta::window {

// A small speech-bubble popup anchored above the character, reused across
// three modes:
//   - typing a message: a real (non-layered, rounded-region) Edit control,
//     since a layered window updated via UpdateLayeredWindow cannot
//     composite normal child controls on top of it — a real Win32
//     limitation, not a styling choice.
//   - waiting for a reply / showing the reply: no child control at all;
//     the rounded bubble, tail, soft shadow, and text are all drawn with
//     GDI+ onto a layered window, since none of it needs to be editable.
// One window is created once and reused rather than recreated per message.
class ChatBubble {
public:
    using SubmitCallback = std::function<void(const std::wstring& message)>;
    using DismissCallback = std::function<void()>;

    static std::unique_ptr<ChatBubble> Create(HINSTANCE instance);
    ~ChatBubble();

    ChatBubble(const ChatBubble&) = delete;
    ChatBubble& operator=(const ChatBubble&) = delete;

    // anchorTop: screen point the bubble's tail should point at (typically
    // the character window's horizontal center, top edge).
    void OpenForInput(POINT anchorTop, SubmitCallback onSubmit, DismissCallback onDismiss);
    void ShowThinking(POINT anchorTop);
    void ShowResponse(POINT anchorTop, const std::wstring& text, DismissCallback onAutoDismiss);
    void Hide();
    bool IsVisible() const;

private:
    ChatBubble(HWND hwnd, HWND edit, HFONT editFont, HBRUSH editBackgroundBrush);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void HandleEditReturn();
    void HandleEditEscape();
    void EnterInputMode(POINT anchorTop);
    void EnterStaticMode(POINT anchorTop, const std::wstring& text);
    void PaintStaticBubble(int width, int height, POINT screenPos, const std::wstring& text);

    HWND hwnd_;
    HWND edit_;
    HFONT editFont_;
    HBRUSH editBackgroundBrush_;
    SubmitCallback onSubmit_;
    DismissCallback onDismiss_;
};

} // namespace sveta::window
