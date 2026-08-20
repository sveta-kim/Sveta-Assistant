#pragma once

#include <string>
#include <string_view>

namespace sveta::core {

std::wstring Utf8ToWide(std::string_view utf8);
std::string WideToUtf8(std::wstring_view wide);

} // namespace sveta::core
