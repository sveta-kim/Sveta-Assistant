#pragma once

#include <string>

#include "character/Personality.h"

namespace sveta::ai {

// Minimal persona injection until the real Character Package / dialogue
// rules system exists (project plan section 25). Keeps replies short and
// colored by the character's personality parameters.
// userName / relationshipNote / primaryLanguage: user-facing
// personalization (see ai/UserProfile.h) -- userName/relationshipNote are
// omitted from the prompt when empty rather than instructing the AI with
// an empty value; primaryLanguage always contributes an instruction
// (defaults to "Korean").
std::string BuildSystemPrompt(
    const character::Personality& personality, const std::string& userName = "",
    const std::string& relationshipNote = "", const std::string& primaryLanguage = "Korean");

} // namespace sveta::ai
