#include "core/Paths.h"

#include <windows.h>

namespace sveta::core {

std::filesystem::path LocalAppDataDir() {
    wchar_t* localAppData = nullptr;
    size_t len = 0;
    _wdupenv_s(&localAppData, &len, L"LOCALAPPDATA");
    std::filesystem::path base = localAppData ? std::filesystem::path(localAppData) : std::filesystem::temp_directory_path();
    free(localAppData);

    std::filesystem::path dir = base / L"SvetaAssistant";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace sveta::core
