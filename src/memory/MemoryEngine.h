#pragma once

#include <memory>
#include <string>

#include "memory/DailyMemory.h"
#include "memory/LongTermMemory.h"

namespace sveta::memory {

// Orchestrates Phase 8 Memory (project plan section 22): owns today's
// Daily Memory log and the cumulative Long-Term Memory store, and turns
// signals MainWindow already polls every tick (game detection, dev-tool
// focus, idle/sleep state) into timestamped daily entries plus long-term
// usage stats -- no AI calls involved, matching this codebase's style of
// keeping internal bookkeeping deterministic.
class MemoryEngine {
public:
    // Loads PrivacyConfig itself (same pattern as ContextEngine::Create)
    // and prunes old daily files.
    static std::unique_ptr<MemoryEngine> Create();

    // Live update from the Settings window; while disabled, Tick() records
    // nothing new and BuildMemoryDigestLine() returns empty, though
    // already-recorded data on disk is left alone (re-enabling picks back
    // up where it left off).
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    void RecordAppStart();

    // Call from the same site MainWindow consumes
    // ContextEngine::ConsumeSameErrorRepeatedEvent() -- one entry per
    // actual occurrence, no extra debouncing needed since that event is
    // already one-shot.
    void RecordSameErrorRepeated();

    // Call once per second (MainWindow::HandleTick already runs on a
    // 1-second timer). Internally debounces game/dev-tool/break state so a
    // few-second alt-tab doesn't spam the daily log with start/end pairs.
    void Tick(bool isGaming, const std::wstring& processName, bool isSleeping);

    // Flushes Long-Term Memory (Daily Memory writes through on every
    // entry already). Call from WM_DESTROY alongside SaveCurrentPosition.
    void Save() const;

    // Short digest folded into the AI system prompt as an extra message,
    // same idiom as ContextEngine::BuildContextLine() -- empty if memory
    // is disabled.
    std::string BuildMemoryDigestLine() const;

private:
    explicit MemoryEngine(bool enabled);

    // Shared shape for both the game and dev-tool start/end trackers:
    // holds a "candidate" state seen on the most recent tick(s) and the
    // "committed" state actually reflected in the daily log, only
    // promoting candidate -> committed (and logging the transition) once
    // it's held steady for kDebounceTicks in a row.
    struct ProcessActivityTracker {
        bool candidateActive = false;
        std::string candidateProcess;
        int candidateTicks = 0;
        bool committedActive = false;
        std::string committedProcess;
        int committedDurationTicks = 0;
    };
    void TickProcessActivity(
        ProcessActivityTracker& tracker, bool isActive, const std::string& processName, const char* label);

    struct BreakActivityTracker {
        bool candidateActive = false;
        int candidateTicks = 0;
        bool committedActive = false;
    };
    void TickBreakActivity(bool isSleeping);

    bool enabled_;
    DailyMemory today_;
    LongTermMemory longTerm_;

    ProcessActivityTracker gameActivity_;
    ProcessActivityTracker devToolActivity_;
    BreakActivityTracker breakActivity_;

    int ticksSinceLastSave_ = 0;
};

} // namespace sveta::memory
