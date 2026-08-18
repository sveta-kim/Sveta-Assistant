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
};

std::string_view ToString(Action action);

} // namespace sveta::character
