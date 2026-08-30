#include "context/LeagueLiveClient.h"

#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include "context/ActiveWindowTracker.h"
#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::context {

namespace {

constexpr DWORD kTimeoutMs = 1500;
constexpr wchar_t kHost[] = L"127.0.0.1";
constexpr INTERNET_PORT kPort = 2999;
constexpr wchar_t kPath[] = L"/liveclientdata/allgamedata";

class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~WinHttpHandle() {
        if (handle_) {
            WinHttpCloseHandle(handle_);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    HINTERNET Get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_;
};

// Riot's Riot ID rollout means some client versions report an empty
// summonerName and populate riotIdGameName/riotIdTagLine instead; try
// both so matching the active player against allPlayers still works.
// The real HTTP fetch below takes ~2s to fail when nothing's listening
// (observed: WinHTTP's connect/send timeouts don't fail fast on a closed
// loopback port the way a bare TCP connect would) -- unacceptable to
// pay on every single chat message when the user isn't even in a League
// match. This cheap, no-network check (just the foreground process name)
// gates the real fetch so that cost is paid only when it might matter.
bool IsForegroundProcessLeagueOfLegends() {
    const auto info = ActiveWindowTracker::GetCurrentWindowInfo();
    return _wcsicmp(info.processName.c_str(), L"leagueoflegends.exe") == 0 ||
        _wcsicmp(info.processName.c_str(), L"league of legends.exe") == 0;
}

std::wstring PlayerIdentity(const nlohmann::json& player) {
    const std::string riotName = player.value("riotIdGameName", "");
    if (!riotName.empty()) {
        const std::string tag = player.value("riotIdTagLine", "");
        return core::Utf8ToWide(tag.empty() ? riotName : riotName + "#" + tag);
    }
    return core::Utf8ToWide(player.value("summonerName", ""));
}

} // namespace

std::optional<LeagueLiveMatchState> ParseLeagueLiveMatchState(const std::string& allGameDataJson) {
    try {
        const auto parsed = nlohmann::json::parse(allGameDataJson);

        LeagueLiveMatchState state;
        state.gameTimeSeconds = parsed.value("gameData", nlohmann::json::object()).value("gameTime", 0.0);

        const auto activePlayer = parsed.value("activePlayer", nlohmann::json::object());
        state.activePlayerName = PlayerIdentity(activePlayer);
        state.activePlayerLevel = activePlayer.value("level", 0);
        state.activePlayerGold = static_cast<int>(activePlayer.value("currentGold", 0.0));

        for (const auto& player : parsed.value("allPlayers", nlohmann::json::array())) {
            LeagueLivePlayer entry;
            entry.displayName = PlayerIdentity(player);
            entry.championName = core::Utf8ToWide(player.value("championName", ""));
            entry.team = core::Utf8ToWide(player.value("team", ""));
            entry.level = player.value("level", 0);
            entry.isDead = player.value("isDead", false);

            const auto scores = player.value("scores", nlohmann::json::object());
            entry.kills = scores.value("kills", 0);
            entry.deaths = scores.value("deaths", 0);
            entry.assists = scores.value("assists", 0);
            entry.creepScore = scores.value("creepScore", 0);

            if (!state.activePlayerName.empty() && entry.displayName == state.activePlayerName) {
                state.activePlayerChampion = entry.championName;
                state.activePlayerTeam = entry.team;
                for (const auto& item : player.value("items", nlohmann::json::array())) {
                    state.activePlayerItems.push_back(core::Utf8ToWide(item.value("displayName", "")));
                }
            }

            state.players.push_back(std::move(entry));
        }

        return state;
    } catch (const nlohmann::json::exception& e) {
        core::Logger::Warn(std::string("LeagueLiveClient: failed to parse allgamedata: ") + e.what());
        return std::nullopt;
    }
}

std::optional<LeagueLiveMatchState> FetchLeagueLiveMatchState() {
    if (!IsForegroundProcessLeagueOfLegends()) {
        return std::nullopt;
    }

    WinHttpHandle session(WinHttpOpen(
        L"SvetaAssistant/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        return std::nullopt;
    }
    WinHttpSetTimeouts(session.Get(), kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

    WinHttpHandle connect(WinHttpConnect(session.Get(), kHost, kPort, 0));
    if (!connect) {
        return std::nullopt;
    }

    WinHttpHandle request(WinHttpOpenRequest(
        connect.Get(), L"GET", kPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (!request) {
        return std::nullopt;
    }

    // The Live Client Data API always serves a self-signed localhost
    // cert; ignoring the validation errors that come with that is the
    // documented/expected way every integration talks to it.
    DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    WinHttpSetOption(request.Get(), WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));

    if (!WinHttpSendRequest(request.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        return std::nullopt; // most commonly: nothing listening -- no match in progress right now
    }
    if (!WinHttpReceiveResponse(request.Get(), nullptr)) {
        return std::nullopt;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request.Get(), WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode != 200) {
        return std::nullopt;
    }

    std::string responseBody;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request.Get(), &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD bytesRead = 0;
        if (!WinHttpReadData(request.Get(), chunk.data(), available, &bytesRead)) {
            break;
        }
        chunk.resize(bytesRead);
        responseBody += chunk;
    }

    return ParseLeagueLiveMatchState(responseBody);
}

std::wstring BuildLeagueContextLine(const LeagueLiveMatchState& state) {
    const int totalSeconds = static_cast<int>(state.gameTimeSeconds);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;

    std::wstring line = L"(사용자는 지금 리그 오브 레전드 실시간 대전 중이다. 진행 시간: " + std::to_wstring(minutes) +
        L"분 " + std::to_wstring(seconds) + L"초.";

    if (!state.activePlayerChampion.empty()) {
        line += L" 내 챔피언: " + state.activePlayerChampion + L" (레벨 " + std::to_wstring(state.activePlayerLevel) +
            L", 골드 " + std::to_wstring(state.activePlayerGold) + L")";
        if (!state.activePlayerItems.empty()) {
            line += L", 아이템: ";
            for (size_t i = 0; i < state.activePlayerItems.size(); ++i) {
                if (i > 0) {
                    line += L", ";
                }
                line += state.activePlayerItems[i];
            }
        }
        line += L".";
    }

    // Bucketed relative to the active player's own team -- not a fixed
    // ORDER/CHAOS mapping, since which literal team is "mine" changes
    // every match. Falls back to the literal team names if the active
    // player couldn't be matched into allPlayers (rather than mislabeling
    // everyone "enemy").
    const bool knowMyTeam = !state.activePlayerTeam.empty();
    std::wstring myTeam;
    std::wstring enemyTeam;
    for (const auto& player : state.players) {
        std::wstring entry = player.championName + L"(레벨 " + std::to_wstring(player.level) + L", " +
            std::to_wstring(player.kills) + L"/" + std::to_wstring(player.deaths) + L"/" +
            std::to_wstring(player.assists) + L", CS " + std::to_wstring(player.creepScore) +
            (player.isDead ? L", 사망 중" : L"") + L")";
        const bool isMyTeam = knowMyTeam && player.team == state.activePlayerTeam;
        std::wstring& team = isMyTeam ? myTeam : enemyTeam;
        if (!team.empty()) {
            team += L" / ";
        }
        team += entry;
    }
    if (knowMyTeam) {
        if (!myTeam.empty()) {
            line += L" 우리 팀: " + myTeam + L".";
        }
        if (!enemyTeam.empty()) {
            line += L" 상대 팀: " + enemyTeam + L".";
        }
    } else if (!enemyTeam.empty()) {
        line += L" 참가자: " + enemyTeam + L".";
    }

    line += L" 이 정보를 바탕으로 물어보면 상황에 맞는 조언(아이템 빌드, 상대 조합 대응, 주의할 상대 등)을 해줘.)";
    return line;
}

} // namespace sveta::context
