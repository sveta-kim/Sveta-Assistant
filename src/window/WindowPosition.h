#pragma once

#include <windows.h>

#include <optional>

namespace sveta::window {

std::optional<POINT> LoadWindowPosition();
void SaveWindowPosition(POINT position);

} // namespace sveta::window
