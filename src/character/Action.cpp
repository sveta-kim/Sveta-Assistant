#include "character/Action.h"

namespace sveta::character {

std::string_view ToString(Action action) {
    switch (action) {
        case Action::Idle: return "Idle";
        case Action::Sitting: return "Sitting";
        case Action::Standing: return "Standing";
        case Action::Walking: return "Walking";
        case Action::Sleeping: return "Sleeping";
        case Action::LookingAtCursor: return "LookingAtCursor";
        case Action::Listening: return "Listening";
        case Action::Thinking: return "Thinking";
        case Action::Talking: return "Talking";
        case Action::BeingPetted: return "BeingPetted";
        case Action::Playing: return "Playing";
        case Action::Reading: return "Reading";
        case Action::Drinking: return "Drinking";
        case Action::Dragged: return "Dragged";
        case Action::UsingItem: return "UsingItem";
    }
    return "Unknown";
}

} // namespace sveta::character
