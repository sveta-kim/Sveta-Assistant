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

// Filename (within a character's assets/ dir) for this emotion's sprite,
// e.g. "happy.png" — matches section 14's "초기: PNG Sprite 기반" naming.
std::string_view SpriteFileName(Emotion emotion);

} // namespace sveta::character
