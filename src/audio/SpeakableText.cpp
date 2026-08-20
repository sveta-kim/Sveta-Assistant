#include "audio/SpeakableText.h"

#include <regex>

namespace sveta::audio {

namespace {

bool IsStrippableSymbol(wchar_t ch) {
    return (ch >= 0x2190 && ch <= 0x21FF) ||  // arrows
           (ch >= 0x2300 && ch <= 0x27BF) ||  // misc technical/shapes/symbols/dingbats
           (ch >= 0x2B00 && ch <= 0x2BFF) ||  // misc symbols and arrows
           (ch >= 0xFE00 && ch <= 0xFE0F) ||  // variation selectors
           (ch == 0x200D);                    // zero-width joiner (emoji sequences)
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
        out.push_back(ch);
    }
    return out;
}

std::wstring StripMarkdown(const std::wstring& text) {
    std::wstring result = text;
    const auto multiline = std::regex_constants::ECMAScript | std::regex_constants::multiline;

    // Fenced code blocks: drop the ``` fence lines, keep the code text.
    result = std::regex_replace(result, std::wregex(L"```[^\n]*\n?"), L"");
    // Inline code, bold, italic: keep the inner text, drop the markers.
    result = std::regex_replace(result, std::wregex(L"`([^`]*)`"), L"$1");
    result = std::regex_replace(result, std::wregex(L"\\*\\*([^*]*)\\*\\*"), L"$1");
    result = std::regex_replace(result, std::wregex(L"__([^_]*)__"), L"$1");
    result = std::regex_replace(result, std::wregex(L"\\*([^*]*)\\*"), L"$1");
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
    return StripEmojiAndSymbols(StripMarkdown(text));
}

} // namespace sveta::audio
