#pragma once

#include <string>
#include <vector>

namespace sveta::context {

// Best-effort discovery of installed Steam games' executable names, so
// GameDetector can recognize a game without the user manually adding it
// to games_config.json. Reads Steam's own local library data -- this
// user's actual installed-games list, not a third-party/scraped
// database, so there's no licensing question about it.
//
// Involves real (if bounded) filesystem I/O across every installed
// game's folder; call this off the UI thread (see GameDetector, which
// runs it on a background thread once at startup).
struct SteamGame {
    std::wstring name;    // display name, e.g. "HELLDIVERS™ 2" -- logging only
    std::wstring exeName; // filename only, e.g. "helldivers2.exe"
};

std::vector<SteamGame> ScanInstalledSteamGames();

} // namespace sveta::context
