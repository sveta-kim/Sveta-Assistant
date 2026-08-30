#include "context/EpicLibraryScanner.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::context {

namespace {

std::optional<std::filesystem::path> FindEpicManifestsDir() {
    wchar_t* programData = nullptr;
    size_t len = 0;
    _wdupenv_s(&programData, &len, L"ProgramData");
    if (!programData) {
        return std::nullopt;
    }
    std::filesystem::path dir =
        std::filesystem::path(programData) / L"Epic" / L"EpicGamesLauncher" / L"Data" / L"Manifests";
    free(programData);
    return dir;
}

} // namespace

std::vector<EpicGame> ScanInstalledEpicGames() {
    std::vector<EpicGame> games;

    const auto manifestsDir = FindEpicManifestsDir();
    if (!manifestsDir) {
        return games;
    }

    std::error_code ec;
    std::filesystem::directory_iterator it(*manifestsDir, ec);
    if (ec) {
        core::Logger::Info("EpicLibraryScanner: no Epic Games manifests directory; skipping");
        return games;
    }

    for (const auto& entry : it) {
        if (entry.path().extension() != L".item") {
            continue;
        }

        std::ifstream file(entry.path());
        if (!file.is_open()) {
            continue;
        }

        try {
            nlohmann::json manifest;
            file >> manifest;

            const std::string launchExecutable = manifest.value("LaunchExecutable", "");
            if (launchExecutable.empty()) {
                continue; // e.g. a DLC/add-on item with nothing to launch directly
            }

            const std::wstring exeName =
                std::filesystem::path(core::Utf8ToWide(launchExecutable)).filename().wstring();
            const std::string displayName = manifest.value("DisplayName", "");
            games.push_back(EpicGame{
                displayName.empty() ? exeName : core::Utf8ToWide(displayName),
                exeName,
            });
        } catch (const nlohmann::json::exception& e) {
            core::Logger::Warn(std::string("EpicLibraryScanner: failed to parse ") + entry.path().string() + ": " + e.what());
        }
    }

    return games;
}

} // namespace sveta::context
