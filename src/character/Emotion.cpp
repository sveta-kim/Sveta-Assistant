#include "character/Emotion.h"

namespace sveta::character {

std::string_view ToString(Emotion emotion) {
    switch (emotion) {
        case Emotion::Calm: return "Calm";
        case Emotion::Happy: return "Happy";
        case Emotion::Excited: return "Excited";
        case Emotion::Curious: return "Curious";
        case Emotion::Sad: return "Sad";
        case Emotion::Annoyed: return "Annoyed";
        case Emotion::Embarrassed: return "Embarrassed";
        case Emotion::Sleepy: return "Sleepy";
        case Emotion::Concerned: return "Concerned";
        case Emotion::Surprised: return "Surprised";
    }
    return "Unknown";
}

std::string_view SpriteFileName(Emotion emotion) {
    switch (emotion) {
        case Emotion::Calm: return "calm.png";
        case Emotion::Happy: return "happy.png";
        case Emotion::Excited: return "excited.png";
        case Emotion::Curious: return "curious.png";
        case Emotion::Sad: return "sad.png";
        case Emotion::Annoyed: return "annoyed.png";
        case Emotion::Embarrassed: return "embarrassed.png";
        case Emotion::Sleepy: return "sleepy.png";
        case Emotion::Concerned: return "concerned.png";
        case Emotion::Surprised: return "surprised.png";
    }
    return "calm.png";
}

} // namespace sveta::character
