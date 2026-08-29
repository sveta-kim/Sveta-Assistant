#pragma once

#include <optional>
#include <string>

namespace sveta::proactive {

// Heuristic for project plan section 18's "동일 오류 반복" (same error
// repeated) event: the only Proactive Assistant trigger implemented so
// far (see InterruptionScore.h for why). Looks for error-ish keywords in
// the active window's title + shallow UI Automation text; if the same
// combined signature shows up again after the user was away from it
// (rather than just staying on screen), that counts as a repeat.
class ErrorRepeatDetector {
public:
    // Call once per fresh (title, uiText) snapshot for the active window.
    // Returns true exactly on a genuine repeat -- not on the first sighting
    // of an error, and not repeatedly while the same error stays on screen.
    bool Observe(const std::wstring& windowTitle, const std::wstring& uiText);

private:
    std::optional<std::wstring> lastErrorSignature_;
    bool errorCurrentlyActive_ = false;
};

} // namespace sveta::proactive
