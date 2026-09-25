#include "strut/diagnostic.h"

#include <sstream>

namespace strut {
std::string format_diagnostic(std::string_view path, const Diagnostic& diagnostic) {
    std::ostringstream out;
    out << path << ':' << diagnostic.span.begin.line << ':' << diagnostic.span.begin.column
        << ": error: " << diagnostic.message;
    return out.str();
}
std::string format_warning(std::string_view path, const Diagnostic& diagnostic) {
    std::ostringstream out;
    out << path << ':' << diagnostic.span.begin.line << ':' << diagnostic.span.begin.column
        << ": warning: " << diagnostic.message;
    return out.str();
}
} // namespace strut
