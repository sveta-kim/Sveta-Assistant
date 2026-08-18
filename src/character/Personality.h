#pragma once

namespace sveta::character {

// Project plan section 12. Each parameter is in [0, 1]; behavior selection
// (idle behavior weighting, reaction intensity) reads these directly so
// personality shows up even when the AI isn't saying anything.
struct Personality {
    float talkativeness = 0.5f;
    float affection = 0.5f;
    float playfulness = 0.5f;
    float curiosity = 0.5f;
    float patience = 0.5f;
    float expressiveness = 0.5f;
    float interruption = 0.5f;
    float shyness = 0.5f;

    // Character Package #001, per section 23/24 — the documented default
    // values until a real character.json loader exists.
    static Personality Sveta();
};

} // namespace sveta::character
