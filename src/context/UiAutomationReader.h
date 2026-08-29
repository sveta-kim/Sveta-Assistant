#pragma once

#include <windows.h>

#include <optional>
#include <string>

namespace sveta::context {

// Project plan section 17: "가능하면 Screenshot보다 구조화된 UI
// 데이터를 우선한다" — prefer structured UI data over screenshots. Reads
// a shallow (immediate children only, not the full descendant tree) text
// summary of a window via UI Automation. Deliberately shallow: walking a
// window's entire UI Automation tree has unbounded latency on some apps
// (large Electron/Chromium UIs in particular), which isn't an acceptable
// risk for something called on every active-window change.
//
// Blocks on COM calls to the target app, so callers should run this off
// the UI thread.
class UiAutomationReader {
public:
    static std::optional<std::wstring> ExtractShallowText(HWND hwnd, int maxElements = 12);
};

} // namespace sveta::context
