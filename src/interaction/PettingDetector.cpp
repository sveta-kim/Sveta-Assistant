#include "interaction/PettingDetector.h"

#include <cmath>

namespace sveta::interaction {

namespace {
// Below this, the cursor is treated as resting rather than stroking.
constexpr double kMinSpeedPxPerSec = 60.0;
// Above this, the movement looks like a pass-through flick, not a stroke.
constexpr double kMaxSpeedPxPerSec = 2000.0;
// Direction reversals must land within this rolling window to count as one
// continuous stroking gesture.
constexpr std::chrono::milliseconds kReversalWindow{1200};
// Reversals needed within the window (~1.5 back-and-forth cycles) before a
// gesture is recognized as petting rather than an incidental wiggle.
constexpr int kMinReversalsInWindow = 3;
} // namespace

bool PettingDetector::OnCursorMove(POINT position, std::chrono::steady_clock::time_point now) {
    if (lastPosition_ && lastMoveTime_) {
        const double dtSeconds = std::chrono::duration<double>(now - *lastMoveTime_).count();
        const int dx = position.x - lastPosition_->x;

        if (dtSeconds > 0.0 && dx != 0) {
            const double speed = std::abs(dx) / dtSeconds;

            // Outside the moderate-speed window, just ignore this sample —
            // don't touch lastDirection_ or reversalTimes_. Real mice fire
            // WM_MOUSEMOVE at high frequency, and Windows coalesces bursts
            // of them under load, so a single jittery too-fast sample is
            // normal noise, not proof the whole stroke wasn't deliberate.
            // Punishing it by clearing progress made genuine petting fail
            // unpredictably depending on message-timing luck.
            if (speed >= kMinSpeedPxPerSec && speed <= kMaxSpeedPxPerSec) {
                const Direction direction = dx > 0 ? Direction::Right : Direction::Left;
                if (lastDirection_ != Direction::None && direction != lastDirection_) {
                    reversalTimes_.push_back(now);
                }
                lastDirection_ = direction;
            }
        }
    }

    while (!reversalTimes_.empty() && now - reversalTimes_.front() > kReversalWindow) {
        reversalTimes_.pop_front();
    }

    lastPosition_ = position;
    lastMoveTime_ = now;

    if (static_cast<int>(reversalTimes_.size()) >= kMinReversalsInWindow) {
        reversalTimes_.clear(); // require a fresh stroke sequence before firing again
        return true;
    }
    return false;
}

void PettingDetector::Reset() {
    lastPosition_.reset();
    lastMoveTime_.reset();
    lastDirection_ = Direction::None;
    reversalTimes_.clear();
}

} // namespace sveta::interaction
