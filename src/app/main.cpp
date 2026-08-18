#include <windows.h>

#include "core/Logger.h"
#include "window/MainWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    sveta::core::Logger::Info("Sveta Assistant starting");

    auto window = sveta::window::MainWindow::Create(instance);
    if (!window) {
        sveta::core::Logger::Error("Failed to initialize main window; exiting");
        return 1;
    }

    const int exitCode = window->RunMessageLoop();

    sveta::core::Logger::Info("Sveta Assistant exiting");
    return exitCode;
}
