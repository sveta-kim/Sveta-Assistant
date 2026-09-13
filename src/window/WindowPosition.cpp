#include "window/WindowPosition.h"

#include <fstream>
#include <sstream>

#include "core/Logger.h"
#include "core/Paths.h"
#include "core/StringConvert.h"

namespace sveta::window {

namespace {

std::filesystem::path PositionFilePath(const std::wstring& fileName) {
    return core::LocalAppDataDir() / fileName;
}

} // namespace

std::optional<POINT> LoadWindowPosition(const std::wstring& fileName) {
    std::ifstream file(PositionFilePath(fileName));
    if (!file.is_open()) {
        return std::nullopt;
    }

    POINT position{};
    file >> position.x >> position.y;
    if (!file) {
        core::Logger::Warn("Ignoring malformed " + core::WideToUtf8(fileName));
        return std::nullopt;
    }

    return position;
}

void SaveWindowPosition(POINT position, const std::wstring& fileName) {
    std::ofstream file(PositionFilePath(fileName), std::ios::trunc);
    if (!file.is_open()) {
        core::Logger::Warn("Failed to save window position to " + core::WideToUtf8(fileName));
        return;
    }
    file << position.x << ' ' << position.y;
}

} // namespace sveta::window
