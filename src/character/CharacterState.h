#pragma once

#include <chrono>
#include <optional>
#include <random>

#include "character/Action.h"
#include "character/Emotion.h"
#include "character/Personality.h"

namespace sveta::character {

// Combines Emotion + Action + Personality + (a slice of) Environment into
// the character's current behavior state (project plan section 9).
class CharacterState {
public:
    explicit CharacterState(Personality personality = Personality::Sveta());

    void OnPetted(std::chrono::steady_clock::time_point now);
    void OnHoverStart(std::chrono::steady_clock::time_point now);
    void OnHoverEnd();
    void OnDragStart(std::chrono::steady_clock::time_point now);
    void OnDragEnd(std::chrono::steady_clock::time_point now, bool isHovering);

    // Call periodically (MainWindow drives this from a timer). Handles the
    // idle-to-sleep timeout, transient-action expiry (e.g. BeingPetted
    // reverting after a couple seconds), and idle behavior selection.
    void Tick(std::chrono::steady_clock::time_point now, bool isHovering);

    Emotion CurrentEmotion() const { return emotion_; }
    Action CurrentAction() const { return action_; }
    const Personality& GetPersonality() const { return personality_; }

private:
    void SetEmotion(Emotion emotion);
    void SetAction(Action action);

    Personality personality_;
    Emotion emotion_ = Emotion::Calm;
    Action action_ = Action::Idle;
    bool isSleeping_ = false;

    std::chrono::steady_clock::time_point lastInteractionTime_;
    std::optional<std::chrono::steady_clock::time_point> transientActionUntil_;

    std::mt19937 rng_;
};

} // namespace sveta::character
