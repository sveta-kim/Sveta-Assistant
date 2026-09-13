#include "audio/SpeakableText.h"

#include <regex>

namespace sveta::audio {

namespace {

bool IsStrippableSymbol(wchar_t ch) {
    return (ch >= 0x2190 && ch <= 0x21FF) ||  // arrows
           (ch >= 0x2300 && ch <= 0x27BF) ||  // misc technical/shapes/symbols/dingbats
           (ch >= 0x2B00 && ch <= 0x2BFF) ||  // misc symbols and arrows
           (ch >= 0xFE00 && ch <= 0xFE0F) ||  // variation selectors
           (ch == 0x200D) ||                  // zero-width joiner (emoji sequences)
           (ch == 0x203C || ch == 0x2049) ||  // ‼ ⁉ (emoji outside the ranges above)
           (ch == 0x2122 || ch == 0x00A9 || ch == 0x00AE); // ™ © ®
}

// Decorative tildes/wave dashes used as a casual sentence-ending flourish
// in Korean chat text ("안녕~") — TTS engines read these literally (e.g.
// "물결표") instead of just pausing. Replaced with a space rather than
// dropped outright: dropping would glue adjacent text together (a numeric
// range like "3~5개" would otherwise become the wrong number "35개").
bool IsTildeLike(wchar_t ch) {
    return ch == L'~' || ch == 0xFF5E || ch == 0x301C || ch == 0x3030 || ch == 0x223C;
}

std::wstring StripEmojiAndSymbols(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < text.size() &&
            text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            ++i; // Skip the low surrogate too. Any supplementary-plane
                 // character (U+10000+) in a casual chat reply is
                 // overwhelmingly likely to be an emoji, so this covers
                 // essentially all of them without a giant range table.
            continue;
        }
        if (IsStrippableSymbol(ch)) {
            continue;
        }
        if (IsTildeLike(ch)) {
            out.push_back(L' ');
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::wstring StripUnrenderableSymbolsImpl(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < text.size() &&
            text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            ++i; // supplementary-plane emoji — see StripEmojiAndSymbols
            continue;
        }
        if (IsStrippableSymbol(ch)) {
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

// Symbol stripping above can leave behind run-together or doubled spaces
// (e.g. two adjacent emoji, or a tilde next to a real space) — collapse
// and trim them so TTS doesn't read out unnatural pauses.
std::wstring CollapseWhitespace(const std::wstring& text) {
    std::wstring result = std::regex_replace(text, std::wregex(L"[ \t]+"), L" ");
    result = std::regex_replace(
        result, std::wregex(L"^[ \t]+|[ \t]+$", std::regex_constants::ECMAScript | std::regex_constants::multiline),
        L"");
    return result;
}

std::wstring StripMarkdown(const std::wstring& text) {
    std::wstring result = text;
    const auto multiline = std::regex_constants::ECMAScript | std::regex_constants::multiline;

    // Fenced code blocks: drop the ``` fence lines, keep the code text.
    result = std::regex_replace(result, std::wregex(L"```[^\n]*\n?"), L"");
    // Inline code, bold: keep the inner text, drop the markers.
    result = std::regex_replace(result, std::wregex(L"`([^`]*)`"), L"$1");
    result = std::regex_replace(result, std::wregex(L"\\*\\*([^*]*)\\*\\*"), L"$1");
    result = std::regex_replace(result, std::wregex(L"__([^_]*)__"), L"$1");
    // Single-asterisk spans are this persona's roleplay action/stage
    // direction convention (e.g. "*조심스럽게 한 모금 마시며*"), not markdown
    // italics -- drop the whole span (markers AND text) so TTS doesn't
    // narrate the character's actions out loud. Must run after the
    // double-asterisk pass above so "**bold**" isn't consumed by this first.
    result = std::regex_replace(result, std::wregex(L"\\*([^*]*)\\*"), L"");
    // Links: keep the link text, drop the URL.
    result = std::regex_replace(result, std::wregex(L"\\[([^\\]]*)\\]\\([^)]*\\)"), L"$1");
    // Headers, blockquotes, list bullets at the start of a line.
    result = std::regex_replace(result, std::wregex(L"^#{1,6}\\s*", multiline), L"");
    result = std::regex_replace(result, std::wregex(L"^>\\s*", multiline), L"");
    result = std::regex_replace(result, std::wregex(L"^[*\\-+]\\s+", multiline), L"");

    return result;
}

} // namespace

std::wstring MakeSpeakable(const std::wstring& text) {
    return CollapseWhitespace(StripEmojiAndSymbols(StripMarkdown(text)));
}

std::wstring StripUnrenderableSymbols(const std::wstring& text) {
    return CollapseWhitespace(StripUnrenderableSymbolsImpl(text));
}

} // namespace sveta::audio
