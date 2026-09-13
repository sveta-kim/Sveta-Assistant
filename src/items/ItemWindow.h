#pragma once

#include <windows.h>

#include <functional>
#include <memory>

#include "rendering/Sprite.h"

namespace sveta::items {

// A small always-on-top layered popup window for a single draggable prop
// (Phase 9 "Item System", scoped down to one item: the coffee mug). Mirrors
// window::ChatBubble's own-window-class idiom and window::MainWindow's
// borderless-caption-drag trick. Unlike ChatBubble, it is NOT glued to the
// character window on every move -- it's independently draggable and stays
// wherever the user drops it; the character window's rect is only read once
// at construction (to pick a starting position) and again on every drag-end
// (to test for overlap).
class ItemWindow {
public:
    // Invoked once per drag when the item's window rect overlaps the
    // character window's rect at drag-end ("offer"). A drop anywhere else is
    // just a reposition: silently persisted, no callback.
    using OfferCallback = std::function<void()>;

    // Returns nullptr if the sprite is missing or window creation fails --
    // the app should continue without the item rather than fail to start.
    static std::unique_ptr<ItemWindow> Create(HINSTANCE instance, HWND characterHwnd, OfferCallback onOffered);
    ~ItemWindow();

    ItemWindow(const ItemWindow&) = delete;
    ItemWindow& operator=(const ItemWindow&) = delete;

    void Show();

    // Persists the window's current on-screen position. Called on every
    // drag-end and once more from MainWindow's WM_DESTROY handler, mirroring
    // MainWindow::SaveCurrentPosition.
    void SaveCurrentPosition() const;

private:
    ItemWindow(HWND hwnd, HWND characterHwnd, rendering::Sprite sprite, OfferCallback onOffered);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ApplySpriteToWindow();
    void HandleDragEnd();

    HWND hwnd_;
    HWND characterHwnd_;
    rendering::Sprite sprite_;
    OfferCallback onOffered_;
};

} // namespace sveta::items
