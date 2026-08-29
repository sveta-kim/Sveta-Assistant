#pragma once

namespace sveta::proactive {

// Project plan section 18 (Proactive Assistant). Only SameErrorRepeated
// is actually wired up to a detector right now (see ErrorRepeatDetector)
// -- the current desktop-awareness infrastructure (window title + shallow
// UI Automation text, no build-system/IDE integration) can't reliably
// tell "task succeeded" or "critical problem" apart from a plain window
// change. The rest of the table is kept here as the reference for
// whichever triggers get implemented next.
enum class ProactiveEvent {
    ScreenChanged,     // 일반 화면 변경
    AppLaunched,       // 앱 실행
    TaskSucceeded,     // 작업 성공
    NewError,          // 새 오류
    SameErrorRepeated, // 동일 오류 반복
    CriticalProblem,   // 중대한 문제
};

float InterruptionScore(ProactiveEvent event);

// Only events scoring at or above this become proactive-speech candidates.
constexpr float kInterruptionThreshold = 0.75f;

} // namespace sveta::proactive
