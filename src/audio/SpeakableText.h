#pragma once

#include <string>

namespace sveta::audio {

// Strips Markdown syntax (**, `, #, [text](url), list/quote markers) and
// emoji/pictographic symbols from AI-generated text so SAPI doesn't read
// literal asterisks, hashes, or pictographs aloud. Only affects what gets
// spoken — the chat bubble still displays the original text.
std::wstring MakeSpeakable(const std::wstring& text);

// Strips just the emoji/pictographic symbols (not Markdown, not tildes)
// from text before it's drawn in the chat bubble. GDI+'s "Segoe UI" font
// has no color-emoji glyphs, so any emoji in an AI reply would otherwise
// render as a broken "tofu" box instead of being silently invisible.
std::wstring StripUnrenderableSymbols(const std::wstring& text);

} // namespace sveta::audio
