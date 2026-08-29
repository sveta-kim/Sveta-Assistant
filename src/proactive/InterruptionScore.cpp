#include "proactive/InterruptionScore.h"

namespace sveta::proactive {

float InterruptionScore(ProactiveEvent event) {
    switch (event) {
        case ProactiveEvent::ScreenChanged: return 0.05f;
        case ProactiveEvent::AppLaunched: return 0.10f;
        case ProactiveEvent::TaskSucceeded: return 0.30f;
        case ProactiveEvent::NewError: return 0.60f;
        case ProactiveEvent::SameErrorRepeated: return 0.82f;
        case ProactiveEvent::CriticalProblem: return 0.95f;
    }
    return 0.0f;
}

} // namespace sveta::proactive
