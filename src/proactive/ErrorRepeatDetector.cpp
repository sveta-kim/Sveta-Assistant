#include "proactive/ErrorRepeatDetector.h"

#include <algorithm>
#include <array>
#include <cwctype>

namespace sveta::proactive {

namespace {

constexpr std::array<const wchar_t*, 8> kErrorKeywords = {
    L"error", L"exception", L"failed", L"fatal", L"crash", L"오류", L"실패", L"에러",
};

std::wstring ToLower(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return std::towlower(c); });
    return text;
}

bool ContainsErrorKeyword(const std::wstring& text) {
    const std::wstring lower = ToLower(text);
    for (const wchar_t* keyword : kErrorKeywords) {
        if (lower.find(ToLower(keyword)) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

bool ErrorRepeatDetector::Observe(const std::wstring& windowTitle, const std::wstring& uiText) {
    const std::wstring combined = windowTitle + L" | " + uiText;

    if (!ContainsErrorKeyword(combined)) {
        errorCurrentlyActive_ = false;
        return false;
    }

    // Only a genuine return to the same signature counts -- not the same
    // error dialog simply still being on screen from one snapshot to the
    // next.
    const bool isRepeat = !errorCurrentlyActive_ && lastErrorSignature_ && *lastErrorSignature_ == combined;

    lastErrorSignature_ = combined;
    errorCurrentlyActive_ = true;
    return isRepeat;
}

} // namespace sveta::proactive
