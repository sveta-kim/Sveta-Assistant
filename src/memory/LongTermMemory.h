#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sveta::memory {

// Project plan section 22, Long-Term Memory. v1 scope: cumulative usage
// stats built from signals the app already detects (process focus time,
// game-detection results) -- NOT "important conversations"/habit
// inference, which would need AI summarization this codebase deliberately
// avoids for internal bookkeeping. That's a documented future extension
// (see the TODO below), not attempted here.
//
// TODO(Phase 8.x): important-conversation / habit extraction. Would need
// either an AI summarization pass over conversationHistory_, or much
// richer heuristics than a tick-based tally -- deferred.
class LongTermMemory {
public:
    static LongTermMemory Load();
    void Save() const;

    // Called once per second from MemoryEngine::Tick() for whatever
    // ContextEngine::CurrentProcessNameForMemory() returns (skipped if
    // empty -- memory disabled or process excluded). Also tallies into the
    // games bucket when isGaming is true, reusing the existing game
    // detection signal rather than a separate detector.
    void RecordActiveSecond(const std::string& processName, bool isGaming);

    int DaysSinceFirstLaunch() const;

    // Highest-N entries by accumulated seconds, name only (the digest just
    // needs "자주 쓰는 프로그램: a, b, c", not exact durations).
    std::vector<std::string> TopPrograms(int n) const;
    std::vector<std::string> TopGames(int n) const;

private:
    std::string firstLaunchDate_; // "YYYY-MM-DD", local calendar date
    std::unordered_map<std::string, int64_t> processActiveSeconds_;
    std::unordered_map<std::string, int64_t> gamesActiveSeconds_;
};

} // namespace sveta::memory
