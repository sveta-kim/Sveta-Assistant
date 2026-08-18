#include "behavior/IdleBehavior.h"

#include <algorithm>
#include <array>
#include <vector>

namespace sveta::behavior {

namespace {
// Chance per tick that *any* idle behavior fires at baseline expressiveness.
constexpr double kBaseFireChance = 0.12;

struct WeightedKind {
    IdleBehaviorKind kind;
    double weight;
};
} // namespace

std::string_view ToString(IdleBehaviorKind kind) {
    switch (kind) {
        case IdleBehaviorKind::LookAround: return "LookAround";
        case IdleBehaviorKind::Blink: return "Blink";
        case IdleBehaviorKind::Yawn: return "Yawn";
        case IdleBehaviorKind::Stretch: return "Stretch";
        case IdleBehaviorKind::Move: return "Move";
        case IdleBehaviorKind::SitAtBottom: return "SitAtBottom";
        case IdleBehaviorKind::Read: return "Read";
        case IdleBehaviorKind::Drink: return "Drink";
        case IdleBehaviorKind::Doze: return "Doze";
        case IdleBehaviorKind::PlayWithItem: return "PlayWithItem";
    }
    return "Unknown";
}

std::optional<IdleBehaviorKind> MaybePickIdleBehavior(const character::Personality& personality, std::mt19937& rng) {
    // Expressiveness makes the character fidget more often overall.
    const double fireChance = std::min(1.0, kBaseFireChance * (0.5 + personality.expressiveness));
    std::bernoulli_distribution shouldFire(fireChance);
    if (!shouldFire(rng)) {
        return std::nullopt;
    }

    const std::array<WeightedKind, 10> options{{
        {IdleBehaviorKind::LookAround, 1.0 + personality.curiosity},
        {IdleBehaviorKind::Blink, 1.0},
        {IdleBehaviorKind::Yawn, 1.0 - personality.playfulness * 0.5},
        {IdleBehaviorKind::Stretch, 1.0},
        {IdleBehaviorKind::Move, 0.5 + personality.playfulness},
        {IdleBehaviorKind::SitAtBottom, 0.5},
        {IdleBehaviorKind::Read, 0.5 + personality.patience * 0.5},
        {IdleBehaviorKind::Drink, 0.5},
        {IdleBehaviorKind::Doze, std::max(0.1f, 1.0f - personality.expressiveness)},
        {IdleBehaviorKind::PlayWithItem, 0.5 + personality.playfulness},
    }};

    std::vector<double> weights;
    weights.reserve(options.size());
    for (const auto& option : options) {
        weights.push_back(std::max(0.01, option.weight));
    }

    std::discrete_distribution<size_t> pick(weights.begin(), weights.end());
    return options[pick(rng)].kind;
}

} // namespace sveta::behavior
