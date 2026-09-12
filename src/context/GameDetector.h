#pragma once

#include <windows.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sveta::context {

// Not a project plan section on its own — a lightweight extension of
// Phase 6 desktop awareness that lets CharacterState "play along" while
// the user is gaming, without the full Phase 40 Game Integration SDK
// (which would need per-game hooks pushing real match/event data).
// Loaded from config/games_config.json (tracked, user-editable — the
// process-name list can't realistically cover every game).
struct GamesConfig {
    std::vector<std::wstring> knownProcessNames;
    // Process names the fullscreen heuristic (see IsLikelyGame) should
    // never fire for, even though they can legitimately go fullscreen
    // (video players, browsers, presentation software, ...). Doesn't
    // affect knownProcessNames/library-scan matches -- those are explicit
    // and always win.
    std::vector<std::wstring> fullscreenHeuristicExclusions;
    static GamesConfig Load();
};

class GameDetector {
public:
    GameDetector();

    // True if processName matches the known-games list or the installed
    // Steam/Epic library (see SteamLibraryScanner, EpicLibraryScanner), OR
    // hwnd looks like a borderless-fullscreen window (covers its monitor,
    // taskbar area included) — most games run one way or the other; a
    // normal maximized app (browser, IDE) stays within the work area and
    // fails that check.
    bool IsLikelyGame(const std::wstring& processName, HWND hwnd) const;

private:
    GamesConfig config_;

    // The Steam/Epic library scans involve real filesystem I/O across
    // every installed game's folder, so they run on a background thread
    // started from the constructor. This shared, mutex-guarded cell
    // (rather than capturing `this`) is what lets that thread safely hand
    // results back even if the GameDetector itself is destroyed first —
    // same shared_ptr-indirection idiom ContextEngine::Create uses for
    // its tracker callback.
    struct LibraryScanState {
        std::mutex mutex;
        std::vector<std::wstring> processNames;
    };
    std::shared_ptr<LibraryScanState> libraryScanState_ = std::make_shared<LibraryScanState>();
};

} // namespace sveta::context
