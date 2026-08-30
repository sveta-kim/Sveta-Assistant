#pragma once

#include <string>
#include <vector>

namespace sveta::context {

// Same idea as SteamLibraryScanner, for the Epic Games Launcher. Reads
// this user's own local install manifests -- no third-party database.
//
// Epic writes one JSON file per installed item to
// %ProgramData%\Epic\EpicGamesLauncher\Data\Manifests\*.item (the .item
// extension is misleading -- it's plain JSON), each carrying a
// LaunchExecutable field naming the real game exe directly. That's a
// stronger signal than Steam's manifest gives us (which only names an
// install folder, not the actual exe), so unlike SteamLibraryScanner
// this doesn't need a bounded directory walk or a noise filter.
//
// Involves filesystem I/O; call this off the UI thread (see
// GameDetector, which runs it on a background thread once at startup).
struct EpicGame {
    std::wstring name;    // display name, e.g. "Fortnite" -- logging only
    std::wstring exeName; // filename only, e.g. "FortniteClient-Win64-Shipping.exe"
};

std::vector<EpicGame> ScanInstalledEpicGames();

} // namespace sveta::context
