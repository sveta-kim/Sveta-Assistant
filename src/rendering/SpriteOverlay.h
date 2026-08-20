#pragma once

#include <cstdint>
#include <vector>

#include "rendering/Sprite.h"

namespace sveta::rendering {

// Returns a copy of the sprite's pixels with a small animated "talking"
// indicator (a tiny 3-bar sound wave) composited near the face. This is a
// runtime overlay rather than pre-baked art: no per-emotion talking frames
// exist yet, and Action::Talking is orthogonal to whichever Emotion
// sprite currently happens to be showing. Real mouth animation is
// mid-term work once character art has separate layers (section 14).
std::vector<uint8_t> WithTalkingIndicator(const Sprite& sprite, bool frameA);

} // namespace sveta::rendering
