#include "window/WindowPosition.h"

#include <fstream>
#include <sstream>

#include "core/Logger.h"
#include "core/Paths.h"

namespace sveta::window {

namespace {

std::filesystem::path PositionFilePath() {
    return core::LocalAppDataDir() / L"window_position.txt";
}

} // namespace

std::optional<POINT> LoadWindowPosition() {
    std::ifstream file(PositionFilePath());
    if (!file.is_open()) {
        return std::nullopt;
    }

    POINT position{};
    file >> position.x >> position.y;
    if (!file) {
        core::Logger::Warn("Ignoring malformed window_position.txt");
        return std::nullopt;
    }

    return position;
}

void SaveWindowPosition(POINT position) {
    std::ofstream file(PositionFilePath(), std::ios::trunc);
    if (!file.is_open()) {
        core::Logger::Warn("Failed to save window position");
        return;
    }
    file << position.x << ' ' << position.y;
}

} // namespace sveta::window
