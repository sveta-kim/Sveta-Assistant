#pragma once

#include <string>

namespace sveta::ai {

// User-facing personalization, loaded from config/user_profile.json
// (tracked; no secrets). Edited via the Settings window (see
// window/SettingsWindow.h).
struct UserProfile {
    // What Sveta should call the user (a nickname, honorific, etc.).
    // Empty means "no preference stated" -- BuildSystemPrompt omits the
    // instruction entirely rather than telling the AI to address the user
    // by an empty name.
    std::string userName;

    // Free-text note on how Sveta should treat/relate to the user (tone,
    // relationship dynamic, formality -- e.g. "반말로 편하게 대해줘" or
    // "여동생처럼 살갑게"). Empty means no preference stated.
    std::string relationshipNote;

    // Sveta's main conversational language (audio::ToString(Language)
    // spelling, e.g. "Korean") -- folded into the system prompt so replies
    // default to it instead of drifting to whatever language the last
    // message happened to be in.
    std::string primaryLanguage = "Korean";

    // Display language for the Settings window itself: "ko" or "en".
    // Doesn't affect the rest of the app (tray menu, chat bubble).
    std::string uiLanguage = "ko";

    static UserProfile Load();
    void Save() const;
};

} // namespace sveta::ai
