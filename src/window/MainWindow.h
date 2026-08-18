#pragma once

#include <windows.h>

#include <memory>
#include <optional>

#include "rendering/Sprite.h"

namespace sveta::window {

// Borderless, always-on-top, per-pixel-transparent window that hosts the
// character sprite. Phase 1 scope: PNG rendering, drag, position save.
class MainWindow {
public:
    static std::unique_ptr<MainWindow> Create(HINSTANCE instance);

    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    int RunMessageLoop();

private:
    MainWindow(HWND hwnd, std::optional<rendering::Sprite> sprite);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void ApplySpriteToWindow();
    void SaveCurrentPosition();

    HWND hwnd_;
    std::optional<rendering::Sprite> sprite_;
};

} // namespace sveta::window
