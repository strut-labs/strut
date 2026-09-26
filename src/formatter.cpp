#include "strut/formatter.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>

namespace strut {
namespace {
std::string trim(const std::string& value) {
    std::size_t a = 0, b = value.size();
    while (a < b && std::isspace(static_cast<unsigned char>(value[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(value[b - 1]))) --b;
    return value.substr(a, b - a);
}

int brace_delta(const std::string& line) {
    bool in_string = false, escaped = false;
    int delta = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (!in_string && c == '/' && i + 1 < line.size() && line[i + 1] == '/') break;
        if (in_string) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') in_string = false;
            continue;
        }
        if (c == '"') { in_string = true; continue; }
        if (c == '{') ++delta;
        else if (c == '}') --delta;
    }
    return delta;
}

bool begins_with_close(const std::string& line) {
    for (char c : line) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        return c == '}';
    }
    return false;
}

std::string canonical_type_spacing(std::string line) {
    static const std::regex pointer_open(R"(\b(ptr|weak_ptr)\s*<\s*)");
    line = std::regex_replace(line, pointer_open, "$1<");
    // Remove whitespace immediately inside generic closers for the canonical pointer spellings.
    static const std::regex pointer_body(R"(\b(ptr|weak_ptr)<([^>]*?)\s+>)");
    line = std::regex_replace(line, pointer_body, "$1<$2>");
    static const std::regex stdlib_generic(R"(\b(atomic|vector|deque|list|map|set|ordered_map|ordered_set|queue|stack|priority_queue|tuple)\s*<\s*([^>]*)\s*>)");
    std::smatch m;
    std::string rest=line,out;
    while(std::regex_search(rest,m,stdlib_generic)){
        out+=m.prefix().str();
        std::string body=m[2].str();
        body=std::regex_replace(body,std::regex(R"(\s*,\s*)"),",");
        while(!body.empty()&&std::isspace(static_cast<unsigned char>(body.back())))body.pop_back();
        out+=m[1].str()+"<"+body+">";
        rest=m.suffix().str();
    }
    return out+rest;
}
}

std::string format_source_text(const std::string& source) {
    std::string normalized;
    normalized.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') continue;
            normalized.push_back('\n');
        } else normalized.push_back(source[i]);
    }

    std::istringstream input(normalized);
    std::ostringstream output;
    std::string line;
    int indent = 0;
    bool previous_blank = false;
    while (std::getline(input, line)) {
        std::string t = trim(line);
        if (t.empty()) {
            if (!previous_blank) output << '\n';
            previous_blank = true;
            continue;
        }
        previous_blank = false;
        if (begins_with_close(t)) indent = std::max(0, indent - 1);
        t = canonical_type_spacing(t);
        output << std::string(static_cast<std::size_t>(indent) * 4, ' ') << t << '\n';
        int delta = brace_delta(t);
        if (begins_with_close(t)) ++delta; // leading close already accounted for above
        indent = std::max(0, indent + delta);
    }
    std::string result = output.str();
    while (result.size() >= 2 && result[result.size()-1] == '\n' && result[result.size()-2] == '\n') result.pop_back();
    if (result.empty() || result.back() != '\n') result.push_back('\n');
    return result;
}
}
