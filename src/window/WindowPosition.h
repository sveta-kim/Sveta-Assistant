#pragma once

#include <windows.h>

#include <optional>
#include <string>

namespace sveta::window {

// fileName is relative to core::LocalAppDataDir(); defaults to the main
// character window's own file so existing call sites are unaffected. A
// second window (e.g. items::ItemWindow) passes its own fileName to get an
// independent saved position.
std::optional<POINT> LoadWindowPosition(const std::wstring& fileName = L"window_position.txt");
void SaveWindowPosition(POINT position, const std::wstring& fileName = L"window_position.txt");

} // namespace sveta::window
