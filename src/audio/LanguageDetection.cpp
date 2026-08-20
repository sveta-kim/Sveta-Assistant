#include "audio/LanguageDetection.h"

namespace sveta::audio {

namespace {

bool InRange(wchar_t ch, wchar_t low, wchar_t high) {
    return ch >= low && ch <= high;
}

bool IsHangul(wchar_t ch) {
    return InRange(ch, 0xAC00, 0xD7A3) ||  // syllables
           InRange(ch, 0x1100, 0x11FF) ||  // Jamo
           InRange(ch, 0x3130, 0x318F);    // compatibility Jamo
}

bool IsKana(wchar_t ch) {
    return InRange(ch, 0x3040, 0x309F) ||  // Hiragana
           InRange(ch, 0x30A0, 0x30FF);    // Katakana
}

bool IsCjkIdeograph(wchar_t ch) {
    return InRange(ch, 0x4E00, 0x9FFF);
}

bool IsCyrillic(wchar_t ch) {
    return InRange(ch, 0x0400, 0x04FF);
}

bool IsGermanSpecific(wchar_t ch) {
    switch (ch) {
        case 0xE4: case 0xF6: case 0xFC: case 0xDF: // ä ö ü ß
        case 0xC4: case 0xD6: case 0xDC:             // Ä Ö Ü
            return true;
        default:
            return false;
    }
}

bool IsSpanishSpecific(wchar_t ch) {
    switch (ch) {
        case 0xF1: case 0xD1: // ñ Ñ
        case 0xBF: case 0xA1: // ¿ ¡
            return true;
        default:
            return false;
    }
}

} // namespace

Language DetectLanguage(const std::wstring& text) {
    // Kana implies Japanese even if CJK ideographs (shared with Chinese)
    // are also present, so it must be checked before the ideograph test.
    bool hasKana = false;
    bool hasCjk = false;
    bool hasGerman = false;
    bool hasSpanish = false;

    for (wchar_t ch : text) {
        if (IsHangul(ch)) {
            return Language::Korean;
        }
        if (IsCyrillic(ch)) {
            return Language::Russian;
        }
        hasKana = hasKana || IsKana(ch);
        hasCjk = hasCjk || IsCjkIdeograph(ch);
        hasGerman = hasGerman || IsGermanSpecific(ch);
        hasSpanish = hasSpanish || IsSpanishSpecific(ch);
    }

    if (hasKana) {
        return Language::Japanese;
    }
    if (hasCjk) {
        return Language::Chinese;
    }
    if (hasGerman) {
        return Language::German;
    }
    if (hasSpanish) {
        return Language::Spanish;
    }
    return Language::English;
}

std::wstring VoiceLcidQuery(Language language) {
    switch (language) {
        case Language::Korean: return L"Language=412";
        case Language::Japanese: return L"Language=411";
        case Language::Chinese: return L"Language=804";  // Simplified (zh-CN)
        case Language::Russian: return L"Language=419";
        case Language::German: return L"Language=407";
        case Language::Spanish: return L"Language=c0a";  // es-ES, international sort
        case Language::English: return L"Language=409";
    }
    return L"Language=409";
}

std::string_view ToString(Language language) {
    switch (language) {
        case Language::Korean: return "Korean";
        case Language::Japanese: return "Japanese";
        case Language::Chinese: return "Chinese";
        case Language::Russian: return "Russian";
        case Language::German: return "German";
        case Language::Spanish: return "Spanish";
        case Language::English: return "English";
    }
    return "Unknown";
}

} // namespace sveta::audio
