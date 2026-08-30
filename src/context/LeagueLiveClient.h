#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sveta::context {

// Real-time coaching context for League of Legends specifically -- the
// user confirmed no other currently-detected game has an equivalent
// official live-match data source (Rainbow Six Siege has none; reading
// game memory to fake one would violate its anti-cheat's terms).
//
// Riot's own League client exposes match state locally while a game is
// in progress (loading screen through the post-game stats screen) at
// https://127.0.0.1:2999/liveclientdata/* -- no auth, self-signed cert,
// nothing listening at all when not in a match. That last part means
// "not currently in a League match" and "request failed" look the same
// from here, which is fine: both mean "no live context available."

struct LeagueLivePlayer {
    std::wstring displayName; // summonerName, or "riotIdGameName#riotIdTagLine" on newer clients
    std::wstring championName;
    std::wstring team; // "ORDER" or "CHAOS"
    int level = 0;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    int creepScore = 0;
    bool isDead = false;
};

struct LeagueLiveMatchState {
    double gameTimeSeconds = 0;
    std::wstring activePlayerName;
    std::wstring activePlayerChampion; // empty if it couldn't be matched in allPlayers
    std::wstring activePlayerTeam;     // "ORDER" or "CHAOS"; empty alongside activePlayerChampion
    int activePlayerLevel = 0;
    int activePlayerGold = 0;
    std::vector<std::wstring> activePlayerItems;
    std::vector<LeagueLivePlayer> players;
};

// Parses a raw /liveclientdata/allgamedata response body. Exposed
// separately from the network fetch below so it can be tested against a
// known-shape sample without needing a live match running.
std::optional<LeagueLiveMatchState> ParseLeagueLiveMatchState(const std::string& allGameDataJson);

// nullopt whenever there's no live match to report on (the normal case
// most of the time) or the request/parse otherwise failed -- not logged
// as an error either way. Blocks on a local HTTP(S) request with a short
// timeout; call off the UI thread.
std::optional<LeagueLiveMatchState> FetchLeagueLiveMatchState();

// Ready-to-use Korean summary for the AI system prompt, mirroring
// ContextEngine::BuildContextLine's shape.
std::wstring BuildLeagueContextLine(const LeagueLiveMatchState& state);

} // namespace sveta::context
