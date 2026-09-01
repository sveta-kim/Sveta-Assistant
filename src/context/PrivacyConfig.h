#pragma once

#include <string>
#include <vector>

namespace sveta::context {

// Project plan section 51 (Privacy): "화면 인식 기능은 사용자가
// 명확하게 통제할 수 있어야 한다" — loaded from config/privacy_config.json
// (tracked; contains no secrets, just user-editable toggles).
struct PrivacyConfig {
    bool screenAwarenessEnabled = true;
    // Process filenames (e.g. "keepass.exe") to never read window
    // title/UI text from. Empty by default — deliberately not guessing a
    // "smart" default list, since a false sense of security is worse than
    // none; the user should add anything sensitive themselves.
    std::vector<std::wstring> excludedProcesses;

    // User-facing on/off switches surfaced in the Settings window (see
    // window/SettingsWindow.h). Grouped here with the toggle above since
    // they're all "how much the character notices/does on its own".
    bool proactiveSpeechEnabled = true;
    bool gameDetectionEnabled = true;

    static PrivacyConfig Load();
    void Save() const;
};

} // namespace sveta::context
