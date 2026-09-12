#include "memory/MemoryEngine.h"

#include <algorithm>
#include <format>

#include "context/PrivacyConfig.h"
#include "core/Logger.h"
#include "core/StringConvert.h"

namespace sveta::memory {

namespace {

// How many consecutive ticks (1/sec, see MainWindow::HandleTick) a new
// game/dev-tool/break state must hold before it's committed to the daily
// log -- avoids logging a "게임 시작"/"게임 종료" pair for a few-second
// alt-tab.
constexpr int kDebounceTicks = 30;

// Only start citing long-term stats in the digest once there's a few
// days of history -- otherwise "자주 쓰는 프로그램" after five minutes of
// first use reads as a non-sequitur.
constexpr int kMinDailyFilesForLongTermDigest = 3;

// Small hardcoded list, same shape as GameDetector.cpp's
// DefaultFullscreenHeuristicExclusions() -- not user-extensible in v1
// (no dedicated config file yet).
// TODO(Phase 8.x): move this to a user-editable config file, mirroring
// games_config.json's fullscreen_heuristic_exclusions.
bool IsDevToolProcess(const std::wstring& processName) {
    static const std::vector<std::wstring> kDevTools = {
        L"devenv.exe",    L"Code.exe",       L"CLion64.exe",     L"idea64.exe",     L"rider64.exe",
        L"pycharm64.exe", L"webstorm64.exe", L"phpstorm64.exe",  L"goland64.exe",   L"rubymine64.exe",
        L"studio64.exe",  L"sublime_text.exe", L"notepad++.exe", L"claude.exe",
    };
    return std::find(kDevTools.begin(), kDevTools.end(), processName) != kDevTools.end();
}

std::string FormatDuration(int totalSeconds) {
    const int minutes = totalSeconds / 60;
    if (minutes < 60) {
        return std::format("{}분", std::max(minutes, 1));
    }
    return std::format("{}시간 {}분", minutes / 60, minutes % 60);
}

std::string JoinWithComma(const std::vector<std::string>& items) {
    std::string joined;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            joined += ", ";
        }
        joined += items[i];
    }
    return joined;
}

} // namespace

MemoryEngine::MemoryEngine(bool enabled) : enabled_(enabled), today_(DailyMemory::LoadToday()), longTerm_(LongTermMemory::Load()) {}

std::unique_ptr<MemoryEngine> MemoryEngine::Create() {
    DailyMemory::PruneOlderThan(30);
    const context::PrivacyConfig privacy = context::PrivacyConfig::Load();
    return std::unique_ptr<MemoryEngine>(new MemoryEngine(privacy.memoryEnabled));
}

void MemoryEngine::RecordAppStart() {
    if (!enabled_) {
        return;
    }
    today_.AppendEntry("프로그램 시작");
}

void MemoryEngine::RecordSameErrorRepeated() {
    if (!enabled_) {
        return;
    }
    today_.AppendEntry("같은 오류 반복 감지");
}

void MemoryEngine::TickProcessActivity(
    ProcessActivityTracker& tracker, bool isActive, const std::string& processName, const char* label) {
    if (isActive == tracker.candidateActive && (!isActive || processName == tracker.candidateProcess)) {
        ++tracker.candidateTicks;
    } else {
        tracker.candidateActive = isActive;
        tracker.candidateProcess = processName;
        tracker.candidateTicks = 1;
    }

    const bool candidateDiffersFromCommitted =
        tracker.candidateActive != tracker.committedActive ||
        (tracker.candidateActive && tracker.candidateProcess != tracker.committedProcess);

    if (candidateDiffersFromCommitted && tracker.candidateTicks >= kDebounceTicks) {
        if (tracker.committedActive) {
            today_.AppendEntry(
                std::format("{} 종료 ({})", label, FormatDuration(tracker.committedDurationTicks)));
        }
        if (tracker.candidateActive) {
            today_.AppendEntry(std::format("{} 시작 ({})", label, tracker.candidateProcess));
        }
        tracker.committedActive = tracker.candidateActive;
        tracker.committedProcess = tracker.candidateProcess;
        tracker.committedDurationTicks = 0;
    }

    if (tracker.committedActive) {
        ++tracker.committedDurationTicks;
    }
}

void MemoryEngine::TickBreakActivity(bool isSleeping) {
    if (isSleeping == breakActivity_.candidateActive) {
        ++breakActivity_.candidateTicks;
    } else {
        breakActivity_.candidateActive = isSleeping;
        breakActivity_.candidateTicks = 1;
    }

    if (breakActivity_.candidateActive != breakActivity_.committedActive &&
        breakActivity_.candidateTicks >= kDebounceTicks) {
        breakActivity_.committedActive = breakActivity_.candidateActive;
        today_.AppendEntry(breakActivity_.committedActive ? "휴식 시작" : "활동 재개");
    }
}

void MemoryEngine::Tick(bool isGaming, const std::wstring& processName, bool isSleeping) {
    if (!enabled_) {
        return;
    }

    const std::string processNameUtf8 = core::WideToUtf8(processName);
    longTerm_.RecordActiveSecond(processNameUtf8, isGaming);

    TickProcessActivity(gameActivity_, isGaming, processNameUtf8, "게임");
    TickProcessActivity(devToolActivity_, !isGaming && IsDevToolProcess(processName), processNameUtf8, "개발");
    TickBreakActivity(isSleeping);

    if (++ticksSinceLastSave_ >= 60) {
        ticksSinceLastSave_ = 0;
        longTerm_.Save();
    }
}

void MemoryEngine::Save() const {
    longTerm_.Save();
}

std::string MemoryEngine::BuildMemoryDigestLine() const {
    if (!enabled_) {
        return "";
    }

    std::string digest = std::format("(함께한 지 {}일째.", longTerm_.DaysSinceFirstLaunch());

    const auto& entries = today_.TodayEntries();
    if (!entries.empty()) {
        digest += " 오늘: ";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) {
                digest += ", ";
            }
            digest += std::format("{} {}", entries[i].time, entries[i].text);
        }
        digest += ".";
    }

    if (DailyMemory::CountDailyFiles() >= kMinDailyFilesForLongTermDigest) {
        const auto topPrograms = longTerm_.TopPrograms(3);
        if (!topPrograms.empty()) {
            digest += std::format(" 자주 쓰는 프로그램: {}.", JoinWithComma(topPrograms));
        }
        const auto topGames = longTerm_.TopGames(3);
        if (!topGames.empty()) {
            digest += std::format(" 자주 하는 게임: {}.", JoinWithComma(topGames));
        }
    }

    digest += ")";
    return digest;
}

} // namespace sveta::memory
