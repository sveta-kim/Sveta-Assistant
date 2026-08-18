#pragma once

#include <windows.h>

#include <memory>

namespace sveta::window {

// Borderless, always-on-top, layered window that will host the character.
// Phase 0/1 scope only: window creation + message loop.
// PNG sprite rendering (Phase 1 continuation) is not implemented yet.
class MainWindow {
public:
    static std::unique_ptr<MainWindow> Create(HINSTANCE instance);

    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    int RunMessageLoop();

private:
    explicit MainWindow(HWND hwnd);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HWND hwnd_;
};

} // namespace sveta::window
