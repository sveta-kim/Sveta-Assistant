#include <windows.h>
#include <objbase.h>

#include "core/Logger.h"
#include "window/MainWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    sveta::core::Logger::Info("Sveta Assistant starting");

    // Apartment-threaded for the whole app lifetime: TextToSpeech's
    // ISpVoice is created once and used repeatedly from this (UI) thread.
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comInit);

    auto window = sveta::window::MainWindow::Create(instance);
    if (!window) {
        sveta::core::Logger::Error("Failed to initialize main window; exiting");
        if (comInitialized) {
            CoUninitialize();
        }
        return 1;
    }

    const int exitCode = window->RunMessageLoop();
    window.reset(); // release COM objects (e.g. ISpVoice) before CoUninitialize

    if (comInitialized) {
        CoUninitialize();
    }

    sveta::core::Logger::Info("Sveta Assistant exiting");
    return exitCode;
}
