#include "context/SteamLibraryScanner.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <optional>

#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::context {

namespace {

// Steam's .vdf/.acf files are a simple key-value format ("key" "value" or
// nested { } blocks) -- this isn't a real VDF parser, just enough
// line-scanning to pull out the handful of keys we actually need.
std::optional<std::string> ExtractQuotedValueForKey(const std::string& line, const std::string& key) {
    const std::string quotedKey = "\"" + key + "\"";
    const auto keyPos = line.find(quotedKey);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }
    const auto firstQuote = line.find('"', keyPos + quotedKey.size());
    if (firstQuote == std::string::npos) {
        return std::nullopt;
    }
    const auto secondQuote = line.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return std::nullopt;
    }
    return line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

std::string UnescapeVdfString(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            result += text[++i];
        } else {
            result += text[i];
        }
    }
    return result;
}

std::optional<std::wstring> ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valueName) {
    HKEY key{};
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    wchar_t buffer[MAX_PATH]{};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    const LONG result = RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || type != REG_SZ) {
        return std::nullopt;
    }
    return std::wstring(buffer);
}

std::optional<std::filesystem::path> FindSteamInstallPath() {
    if (auto path = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath")) {
        return std::filesystem::path(*path);
    }
    return std::nullopt;
}

// libraryfolders.vdf lists every drive Steam has a library on besides the
// main install; the main install itself is always an implicit library.
std::vector<std::filesystem::path> FindLibraryFolders(const std::filesystem::path& steamPath) {
    std::vector<std::filesystem::path> libraries{steamPath};

    std::ifstream file(steamPath / "steamapps" / "libraryfolders.vdf");
    if (!file.is_open()) {
        return libraries;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (auto path = ExtractQuotedValueForKey(line, "path")) {
            libraries.emplace_back(core::Utf8ToWide(UnescapeVdfString(*path)));
        }
    }
    return libraries;
}

// A game's install folder often has several .exe files besides the real
// game (installers, redistributables, crash handlers) -- skip the
// obviously-not-the-game ones by filename pattern. Not exhaustive; false
// positives here are low-stakes (Sveta would just show the gamepad pose
// while, say, a bundled benchmark tool is running).
bool LooksLikeNonGameExe(std::wstring lowerFileName) {
    static constexpr std::array<const wchar_t*, 15> kSkipPatterns = {
        L"unins", L"redist", L"setup", L"crash", L"installer", L"directx", L"eossdk",
        // .NET/PhysX/OpenAL etc. runtime installers -- observed real ones
        // (dotnetfx35.exe, NDP452-KB2901907-x86-x64-AllOS-ENU.exe, ...)
        // slipping past the patterns above during testing.
        L"dotnetfx", L"ndp4", L"ndp3", L"ndp2", L"dxsetup", L"dxwebsetup", L"physx", L"oalinst",
    };
    return std::any_of(kSkipPatterns.begin(), kSkipPatterns.end(), [&lowerFileName](const wchar_t* pattern) {
        return lowerFileName.find(pattern) != std::wstring::npos;
    });
}

// Depth-bounded so a huge asset-heavy game folder doesn't turn "scan the
// library" into a multi-second filesystem walk at startup.
void CollectExeFiles(const std::filesystem::path& dir, int depthRemaining, std::vector<std::wstring>& outExeNames) {
    if (depthRemaining < 0) {
        return;
    }
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        return;
    }
    for (const auto& entry : it) {
        std::error_code entryEc;
        if (entry.is_directory(entryEc)) {
            CollectExeFiles(entry.path(), depthRemaining - 1, outExeNames);
            continue;
        }
        if (!entry.is_regular_file(entryEc)) {
            continue;
        }
        std::wstring extension = entry.path().extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return std::towlower(c); });
        if (extension != L".exe") {
            continue;
        }
        std::wstring fileName = entry.path().filename().wstring();
        std::wstring lowerFileName = fileName;
        std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), [](wchar_t c) { return std::towlower(c); });
        if (!LooksLikeNonGameExe(lowerFileName)) {
            outExeNames.push_back(std::move(fileName));
        }
    }
}

} // namespace

std::vector<SteamGame> ScanInstalledSteamGames() {
    std::vector<SteamGame> games;

    const auto steamPath = FindSteamInstallPath();
    if (!steamPath) {
        core::Logger::Info("SteamLibraryScanner: Steam not found (no HKCU\\Software\\Valve\\Steam\\SteamPath); skipping");
        return games;
    }

    for (const auto& library : FindLibraryFolders(*steamPath)) {
        const auto steamappsDir = library / "steamapps";
        std::error_code ec;
        std::filesystem::directory_iterator it(steamappsDir, ec);
        if (ec) {
            continue;
        }

        for (const auto& entry : it) {
            const std::wstring fileName = entry.path().filename().wstring();
            if (fileName.rfind(L"appmanifest_", 0) != 0 || entry.path().extension() != L".acf") {
                continue;
            }

            std::ifstream manifest(entry.path());
            if (!manifest.is_open()) {
                continue;
            }

            std::optional<std::string> name;
            std::optional<std::string> installDir;
            std::string line;
            while (std::getline(manifest, line)) {
                if (!name) {
                    name = ExtractQuotedValueForKey(line, "name");
                }
                if (!installDir) {
                    installDir = ExtractQuotedValueForKey(line, "installdir");
                }
            }
            if (!installDir) {
                continue;
            }

            const auto gameDir = steamappsDir / "common" / core::Utf8ToWide(*installDir);
            std::vector<std::wstring> exeNames;
            CollectExeFiles(gameDir, /*depthRemaining=*/3, exeNames);

            const std::wstring displayName = name ? core::Utf8ToWide(*name) : core::Utf8ToWide(*installDir);
            for (auto& exeName : exeNames) {
                games.push_back(SteamGame{displayName, std::move(exeName)});
            }
        }
    }

    return games;
}

} // namespace sveta::context
