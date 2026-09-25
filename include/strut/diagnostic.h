#pragma once

#include <string>
#include <string_view>

#include "strut/lexer.h"

namespace strut {

std::string format_diagnostic(std::string_view path, const Diagnostic& diagnostic);

} // namespace strut
