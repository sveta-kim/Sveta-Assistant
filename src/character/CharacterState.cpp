#include "character/CharacterState.h"

#include <format>

#include "behavior/IdleBehavior.h"
#include "core/Logger.h"

namespace sveta::character {

namespace {
// How long without interaction before the character dozes off (project
// plan sections 6, 52). 60s made it doze off mid-task constantly during
// normal desktop use; 5 minutes gives enough room to read/think without
// touching the character before it's treated as "user stepped away".
constexpr std::chrono::minutes kIdleTimeoutToSleep{5};
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

bool CharacterState::IsConversing() const {
    return action_ == Action::Listening || action_ == Action::Thinking || action_ == Action::Talking;
}

Action CharacterState::DefaultAction() const {
    return isGaming_ ? Action::PlayingGame : Action::Idle;
}

Emotion CharacterState::DefaultEmotion() const {
    return isGaming_ ? Emotion::Excited : Emotion::Calm;
}

void CharacterState::OnPetted(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    isSleeping_ = false;
    if (IsConversing()) {
        return; // an incidental pet shouldn't derail an active conversation
    }

    SetEmotion(Emotion::Happy);
    SetAction(Action::BeingPetted);

    // Sveta is documented as enjoying petting (section 8); more affectionate
    // personalities linger in the reaction a bit longer.
    const int durationMs = 1200 + static_cast<int>(personality_.affection * 800.0f);
    transientActionUntil_ = now + std::chrono::milliseconds(durationMs);
}

void CharacterState::OnItemOffered(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    isSleeping_ = false;
    if (IsConversing()) {
        return; // an offer mid-conversation shouldn't derail it (matches OnPetted)
    }

    SetEmotion(Emotion::Happy);
    SetAction(Action::Drinking);
    // Same duration as the existing Drink idle-behavior flourish.
    transientActionUntil_ = now + std::chrono::seconds(3);
}

void CharacterState::OnHoverStart(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    const bool wasSleeping = isSleeping_;
    isSleeping_ = false;

    if (IsConversing()) {
        return;
    }
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
    if (IsConversing() || transientActionUntil_) {
        return;
    }
    if (action_ == Action::LookingAtCursor) {
        SetAction(DefaultAction());
        SetEmotion(DefaultEmotion());
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
    SetAction(isHovering ? Action::LookingAtCursor : DefaultAction());
    SetEmotion(isHovering ? Emotion::Curious : DefaultEmotion());
}

void CharacterState::OnConversationStart(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now;
    isSleeping_ = false;
    transientActionUntil_.reset(); // an explicit chat pre-empts any reaction in progress
    if (emotion_ == Emotion::Sleepy) {
        SetEmotion(Emotion::Calm); // typing a message should visibly wake the character
    }
    SetAction(Action::Listening);
}

void CharacterState::OnThinking() {
    SetAction(Action::Thinking);
}

void CharacterState::OnTalking(std::chrono::steady_clock::time_point now) {
    lastInteractionTime_ = now; // a slow reply shouldn't let the character doze mid-conversation
    SetAction(Action::Talking);
}

void CharacterState::OnConversationEnd(bool isHovering) {
    SetAction(isHovering ? Action::LookingAtCursor : DefaultAction());
    SetEmotion(isHovering ? Emotion::Curious : DefaultEmotion());
}

void CharacterState::SetGamingContext(bool isGaming, std::chrono::steady_clock::time_point now) {
    if (isGaming_ == isGaming) {
        return;
    }
    isGaming_ = isGaming;
    lastInteractionTime_ = now;
    isSleeping_ = false;

    // Don't interrupt a reaction, drag, conversation, or direct hover
    // that's already claiming the display; the new default just takes
    // effect the next time one of those hands control back.
    if (IsConversing() || transientActionUntil_ || action_ == Action::Dragged || action_ == Action::LookingAtCursor) {
        return;
    }

    SetAction(DefaultAction());
    SetEmotion(DefaultEmotion());
}

void CharacterState::Tick(std::chrono::steady_clock::time_point now, bool isHovering) {
    if (transientActionUntil_ && now >= *transientActionUntil_) {
        transientActionUntil_.reset();
        SetAction(isHovering ? Action::LookingAtCursor : DefaultAction());
        SetEmotion(isHovering ? Emotion::Curious : DefaultEmotion());
        return;
    }
    const bool isConversing = action_ == Action::Listening || action_ == Action::Thinking || action_ == Action::Talking;
    if (transientActionUntil_ || action_ == Action::Dragged || isConversing) {
        return; // a reaction, drag, or conversation is already in progress
    }

    if (isGaming_) {
        // Being at the PC playing counts as presence; don't doze off, and
        // hold the PlayingGame pose instead of picking idle flourishes
        // (unless a hover/reaction already has priority — see above).
        lastInteractionTime_ = now;
        if (action_ == Action::Idle) {
            SetAction(Action::PlayingGame);
            SetEmotion(Emotion::Excited);
        }
        return;
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
