#pragma once

#include <windows.h>

#include <chrono>
#include <deque>
#include <optional>

namespace sveta::interaction {

// Recognizes the "petting" gesture from project plan section 8: inside the
// head hitbox, repeated left-right cursor movement at a moderate speed,
// sustained for a short duration. Feed every cursor-move sample while the
// cursor is inside the head hitbox; call Reset() the moment it leaves.
class PettingDetector {
public:
    // Returns true when a petting gesture is (re-)recognized on this call.
    bool OnCursorMove(POINT position, std::chrono::steady_clock::time_point now);
    void Reset();

private:
    enum class Direction { None, Left, Right };

    std::optional<POINT> lastPosition_;
    std::optional<std::chrono::steady_clock::time_point> lastMoveTime_;
    Direction lastDirection_ = Direction::None;
    std::deque<std::chrono::steady_clock::time_point> reversalTimes_;
};

} // namespace sveta::interaction
