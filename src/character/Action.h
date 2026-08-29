#pragma once

#include <string_view>

namespace sveta::character {

// Project plan section 11.
enum class Action {
    Idle,
    Sitting,
    Standing,
    Walking,
    Sleeping,
    LookingAtCursor,
    Listening,
    Thinking,
    Talking,
    BeingPetted,
    Playing,
    Reading,
    Drinking,
    Dragged,
    UsingItem,
    // Not in the original 15 (section 11); added so the character can
    // "play along" while the user is in a game (see context/GameDetector).
    PlayingGame,
};

std::string_view ToString(Action action);

} // namespace sveta::character
