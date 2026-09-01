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
    // expectsSpokenReply: true if a TTS Speak() call for this text has
    // already been kicked off. Widens the fallback auto-dismiss timer
    // (see ShowResponse's .cpp comment) so it acts as a pure safety net
    // instead of racing real playback and cutting the reply off mid-speech.
    void ShowResponse(
        POINT anchorTop, const std::wstring& text, DismissCallback onAutoDismiss, bool expectsSpokenReply = false);
    void Hide();
    bool IsVisible() const;

    // Keeps the bubble anchored to the character while it's being dragged;
    // no-op if the bubble is hidden. Content/size are unchanged.
    void Reposition(POINT anchorTop);

    // Replaces the pending auto-dismiss timer with a fresh one. Used once
    // real TTS playback actually finishes, since the timer ShowResponse
    // starts is only a text-length guess and can fire well before or long
    // after speech really ends.
    void RescheduleDismiss(int delayMs, DismissCallback onDismiss);

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
    void ResizeInputBox(int desiredHeight);
    void HandleEditTextChanged();

    HWND hwnd_;
    HWND edit_;
    HFONT editFont_;
    HBRUSH editBackgroundBrush_;
    SubmitCallback onSubmit_;
    DismissCallback onDismiss_;

    // Anchor and current height of the input box, tracked so it can grow
    // (and reposition upward, staying anchored at the bottom) as the user
    // types a message long enough to wrap onto more lines. 0 forces
    // ResizeInputBox to lay out fresh the next time EnterInputMode runs.
    POINT inputAnchorTop_{};
    int currentInputHeight_ = 0;
};

} // namespace sveta::window
