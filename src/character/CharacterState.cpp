#include "character/CharacterState.h"

#include <format>

#include "behavior/IdleBehavior.h"
#include "core/Logger.h"

namespace sveta::character {

namespace {
// Placeholder pending real UX tuning: how long without interaction before
// the character dozes off (project plan sections 6, 52).
constexpr std::chrono::seconds kIdleTimeoutToSleep{60};
} // namespace

CharacterState::CharacterState(Personality personality)
    : personality_(personality),
      lastInteractionTime_(std::chrono::steady_clock::now()),
      rng_(std::random_device{}()) {}

void CharacterState::SetEmotion(Emotion emotion) {
    if (emotion_ == emotion) {
        return;
    }
    emotion_ = emotion;
    core::Logger::Info(std::format("Emotion -> {}", ToString(emotion_)));
}

void CharacterState::SetAction(Action action) {
    if (action_ == action) {
        return;
    }
    action_ = action;
    core::Logger::Info(std::format("Action -> {}", ToString(action_)));
}

void CharacterState::OnPetted(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    isSleeping_ = false;

    SetEmotion(Emotion::Happy);
    SetAction(Action::BeingPetted);

    // Sveta is documented as enjoying petting (section 8); more affectionate
    // personalities linger in the reaction a bit longer.
    const int durationMs = 1200 + static_cast<int>(personality_.affection * 800.0f);
    transientActionUntil_ = now + std::chrono::milliseconds(durationMs);
}

void CharacterState::OnHoverStart(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    const bool wasSleeping = isSleeping_;
    isSleeping_ = false;

    if (transientActionUntil_) {
        return; // don't interrupt an in-progress reaction (e.g. still being petted)
    }

    // Matches the documented "Curious + LookingAtCursor" combo (section 11).
    SetEmotion(Emotion::Curious);
    SetAction(Action::LookingAtCursor);

    if (wasSleeping) {
        core::Logger::Info("Woke up (cursor approached)");
    }
}

void CharacterState::OnHoverEnd() {
    if (transientActionUntil_) {
        return;
    }
    if (action_ == Action::LookingAtCursor) {
        SetAction(Action::Idle);
        SetEmotion(Emotion::Calm);
    }
}

void CharacterState::OnDragStart(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    isSleeping_ = false;
    transientActionUntil_.reset(); // being grabbed pre-empts any reaction in progress

    SetEmotion(Emotion::Surprised);
    SetAction(Action::Dragged);
}

void CharacterState::OnDragEnd(std::chrono::steady_clock::time_point now, bool isHovering) {
    lastInteractionTime_ = now;
    SetAction(isHovering ? Action::LookingAtCursor : Action::Idle);
    SetEmotion(isHovering ? Emotion::Curious : Emotion::Calm);
}

void CharacterState::Tick(std::chrono::steady_clock::time_point now, bool isHovering) {
    if (transientActionUntil_ && now >= *transientActionUntil_) {
        transientActionUntil_.reset();
        SetAction(isHovering ? Action::LookingAtCursor : Action::Idle);
        SetEmotion(isHovering ? Emotion::Curious : Emotion::Calm);
        return;
    }
    if (transientActionUntil_ || action_ == Action::Dragged) {
        return; // a reaction or an explicit drag is already in progress
    }

    if (!isSleeping_ && now - lastInteractionTime_ >= kIdleTimeoutToSleep) {
        isSleeping_ = true;
        SetAction(Action::Sleeping);
        SetEmotion(Emotion::Sleepy);
        return;
    }
    if (isSleeping_) {
        return; // stays asleep until OnHoverStart/OnDragStart wakes it
    }

    if (action_ != Action::Idle) {
        return; // only pick a new idle flourish while truly idle
    }

    const auto behaviorKind = behavior::MaybePickIdleBehavior(personality_, rng_);
    if (!behaviorKind) {
        return;
    }
    core::Logger::Info(std::format("IdleBehavior: {}", behavior::ToString(*behaviorKind)));

    using behavior::IdleBehaviorKind;
    switch (*behaviorKind) {
        case IdleBehaviorKind::Move:
            SetAction(Action::Walking);
            transientActionUntil_ = now + std::chrono::seconds(2);
            break;
        case IdleBehaviorKind::SitAtBottom:
            SetAction(Action::Sitting);
            transientActionUntil_ = now + std::chrono::seconds(4);
            break;
        case IdleBehaviorKind::Read:
            SetAction(Action::Reading);
            transientActionUntil_ = now + std::chrono::seconds(5);
            break;
        case IdleBehaviorKind::Drink:
            SetAction(Action::Drinking);
            transientActionUntil_ = now + std::chrono::seconds(3);
            break;
        case IdleBehaviorKind::PlayWithItem:
            SetAction(Action::Playing);
            transientActionUntil_ = now + std::chrono::seconds(4);
            break;
        case IdleBehaviorKind::Doze:
            SetEmotion(Emotion::Sleepy);
            transientActionUntil_ = now + std::chrono::seconds(2);
            break;
        case IdleBehaviorKind::LookAround:
        case IdleBehaviorKind::Blink:
        case IdleBehaviorKind::Yawn:
        case IdleBehaviorKind::Stretch:
            // Momentary flourish: logged only, no state/art to reflect it yet.
            break;
    }
}

} // namespace sveta::character
