#pragma once

#include <string_view>

namespace sveta::character {

// Project plan section 10.
enum class Emotion {
    Calm,
    Happy,
    Excited,
    Curious,
    Sad,
    Annoyed,
    Embarrassed,
    Sleepy,
    Concerned,
    Surprised,
};

std::string_view ToString(Emotion emotion);

} // namespace sveta::character
