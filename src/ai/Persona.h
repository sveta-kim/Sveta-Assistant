#pragma once

#include <string>

#include "character/Personality.h"

namespace sveta::ai {

// Minimal persona injection until the real Character Package / dialogue
// rules system exists (project plan section 25). Keeps replies short and
// colored by the character's personality parameters.
std::string BuildSystemPrompt(const character::Personality& personality);

} // namespace sveta::ai
