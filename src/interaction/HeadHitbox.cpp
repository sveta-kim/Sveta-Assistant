#include "interaction/HeadHitbox.h"

namespace sveta::interaction {

namespace {
constexpr double kWidthFraction = 0.72;
constexpr double kHeightFraction = 0.5;
} // namespace

RECT ComputeHeadHitbox(uint32_t spriteWidth, uint32_t spriteHeight) {
    const double marginX = spriteWidth * (1.0 - kWidthFraction) / 2.0;

    RECT rect{};
    rect.left = static_cast<LONG>(marginX);
    rect.right = static_cast<LONG>(spriteWidth - marginX);
    rect.top = 0;
    rect.bottom = static_cast<LONG>(spriteHeight * kHeightFraction);
    return rect;
}

} // namespace sveta::interaction
