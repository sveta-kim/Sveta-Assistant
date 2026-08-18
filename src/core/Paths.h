#pragma once

#include <filesystem>

namespace sveta::core {

// %LOCALAPPDATA%\SvetaAssistant, created on first access if missing.
std::filesystem::path LocalAppDataDir();

} // namespace sveta::core
