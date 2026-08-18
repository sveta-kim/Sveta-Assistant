#pragma once

#include <optional>
#include <random>
#include <string_view>

#include "character/Personality.h"

namespace sveta::behavior {

// Project plan section 13.
enum class IdleBehaviorKind {
    LookAround,
    Blink,
    Yawn,
    Stretch,
    Move,
    SitAtBottom,
    Read,
    Drink,
    Doze,
    PlayWithItem,
};

std::string_view ToString(IdleBehaviorKind kind);

// Called once per idle tick. Usually returns nullopt so behaviors feel
// occasional rather than constant; when one does fire, it's picked with
// weights nudged by personality traits (section 12: personality should
// show up in behavior even when the AI isn't saying anything).
std::optional<IdleBehaviorKind> MaybePickIdleBehavior(
    const character::Personality& personality, std::mt19937& rng);

} // namespace sveta::behavior
