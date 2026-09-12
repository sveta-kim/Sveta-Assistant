#pragma once

#include <string>
#include <vector>

namespace sveta::memory {

// Project plan section 22, Daily Memory: "13:20 개발 시작, 14:10 오류
// 해결, 15:00 휴식, 17:00 다시 개발" -- a short, factual, timestamped
// activity log. Deliberately just event breadcrumbs (no AI summarization,
// matching this codebase's "no API calls for internal bookkeeping" style)
// built from signals the app already detects (see memory/MemoryEngine).
struct DailyMemoryEntry {
    std::string time; // local "HH:MM", stamped at append time
    std::string text;
};

// One day's log, stored at LocalAppDataDir()/memory/daily/YYYY-MM-DD.json.
// Untracked/machine-local (unlike config/*.json) since it grows over time
// and isn't meant to be hand-edited.
class DailyMemory {
public:
    // Loads (or starts empty) today's log, using the local calendar date --
    // core::Logger's timestamps are UTC, which would be wrong for a log a
    // Korean user reads as literal wall-clock time.
    static DailyMemory LoadToday();

    // Appends a new entry stamped with the current local time and writes
    // the file immediately -- entries are infrequent (debounced by
    // MemoryEngine), so write-through simplicity beats batching here.
    void AppendEntry(const std::string& text);

    const std::vector<DailyMemoryEntry>& TodayEntries() const { return entries_; }

    // Deletes daily files older than `days` by filename date. Call once at
    // startup; cheap (filename parsing only, no need to open old files).
    static void PruneOlderThan(int days);

    // How many daily files currently exist -- MemoryEngine uses this as a
    // "is there enough history yet" gate before showing long-term stats in
    // the AI digest, so it doesn't cite "frequently used programs" after
    // five minutes of first use.
    static int CountDailyFiles();

private:
    explicit DailyMemory(std::string date);
    void Save() const;

    std::string date_; // "YYYY-MM-DD", local calendar date
    std::vector<DailyMemoryEntry> entries_;
};

} // namespace sveta::memory
