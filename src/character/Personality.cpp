#include "character/Personality.h"

namespace sveta::character {

Personality Personality::Sveta() {
    Personality p;
    p.talkativeness = 0.55f;
    p.affection = 0.90f;
    p.playfulness = 0.75f;
    p.curiosity = 0.80f;
    p.patience = 0.60f;
    p.expressiveness = 0.85f;
    p.interruption = 0.35f;
    p.shyness = 0.40f;
    return p;
}

} // namespace sveta::character
