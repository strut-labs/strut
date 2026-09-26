#pragma once

#include <string>
#include <string_view>

#include "strut/lexer.h"

namespace strut {

std::string format_diagnostic(std::string_view path, const Diagnostic& diagnostic);
std::string format_warning(std::string_view path, const Diagnostic& diagnostic);
std::string format_diagnostic_with_source(std::string_view path, std::string_view source, const Diagnostic& diagnostic, bool warning = false, bool color = true);

} // namespace strut
