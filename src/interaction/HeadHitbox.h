#pragma once

#include <windows.h>

#include <cstdint>

namespace sveta::interaction {

// Proportional heuristic for the head/helmet region of a character sprite,
// calibrated against the current calm.png artwork (head fills roughly the
// top half, centered). Real per-character hitbox data belongs to the
// future Character Package format (project plan sections 24-25); this
// keeps petting detection unblocked until that exists.
RECT ComputeHeadHitbox(uint32_t spriteWidth, uint32_t spriteHeight);

} // namespace sveta::interaction
