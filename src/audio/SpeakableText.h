#pragma once

#include <string>

namespace sveta::audio {

// Strips Markdown syntax (**, `, #, [text](url), list/quote markers) and
// emoji/pictographic symbols from AI-generated text so SAPI doesn't read
// literal asterisks, hashes, or pictographs aloud. Only affects what gets
// spoken — the chat bubble still displays the original text.
std::wstring MakeSpeakable(const std::wstring& text);

} // namespace sveta::audio
