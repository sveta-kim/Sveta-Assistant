#pragma once

#include <windows.h>

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
    static GamesConfig Load();
};

class GameDetector {
public:
    GameDetector();

    // True if processName matches the known-games list, OR hwnd looks like
    // a borderless-fullscreen window (covers its monitor exactly, no
    // WS_CAPTION) — most games run one way or the other; a maximized
    // normal app (browser, IDE) still has window chrome and fails the
    // second check.
    bool IsLikelyGame(const std::wstring& processName, HWND hwnd) const;

private:
    GamesConfig config_;
};

} // namespace sveta::context
