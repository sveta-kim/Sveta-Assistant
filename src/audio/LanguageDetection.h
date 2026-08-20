#pragma once

#include <string>
#include <string_view>

namespace sveta::audio {

// Project plan section 21 target languages: Korean, English, Japanese,
// Chinese, Spanish, German, Russian.
enum class Language {
    Korean,
    Japanese,
    Chinese,
    Russian,
    German,
    Spanish,
    English, // default/fallback
};

// Cheap script-based guess, not real language identification: distinct
// scripts (Hangul, Kana, CJK ideographs, Cyrillic) are unambiguous, but
// German/Spanish/English all share Latin script, so those two are only
// detected via a few language-specific accented characters — plain Latin
// text with none of those falls through to English.
Language DetectLanguage(const std::wstring& text);

// SAPI voice-selection query string for SpFindBestToken, e.g. "Language=412".
// Whether this actually finds an installed voice depends on what's on the
// machine — see README for which of the 7 are voice-verified here.
std::wstring VoiceLcidQuery(Language language);

std::string_view ToString(Language language);

} // namespace sveta::audio
