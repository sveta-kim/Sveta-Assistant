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

} // namespace sveta::character
