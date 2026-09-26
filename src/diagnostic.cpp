#include "strut/diagnostic.h"

#include <cctype>
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

namespace {
std::string source_line(std::string_view source, std::size_t wanted) {
    if (wanted == 0) return {};
    std::size_t line = 1, start = 0;
    for (std::size_t i = 0; i <= source.size(); ++i) {
        if (i == source.size() || source[i] == '\n') {
            if (line == wanted) {
                std::size_t end = i;
                if (end > start && source[end - 1] == '\r') --end;
                return std::string(source.substr(start, end - start));
            }
            ++line; start = i + 1;
        }
    }
    return {};
}

std::string highlight_line(std::string_view line, bool color) {
    if (!color) return std::string(line);
    constexpr std::string_view reset="\x1b[0m", keyword="\x1b[1;35m", number="\x1b[36m", stringc="\x1b[32m", comment="\x1b[90m";
    std::ostringstream out;
    auto is_ident=[](unsigned char c){return std::isalnum(c)||c=='_';};
    auto is_kw=[](std::string_view w){
        static constexpr std::string_view kws[]={"function","return","if","else","while","for","in","const","include","struct","enum","type","match","switch","case","default","break","continue","try","catch","throw","unsafe","async","await","void","true","false","null"};
        for(auto k:kws)if(w==k)return true;return false;
    };
    for(std::size_t i=0;i<line.size();){
        if(i+1<line.size()&&line[i]=='/'&&line[i+1]=='/'){out<<comment<<line.substr(i)<<reset;break;}
        if(line[i]=='"'){std::size_t j=i+1;bool esc=false;for(;j<line.size();++j){if(!esc&&line[j]=='"'){++j;break;}esc=!esc&&line[j]=='\\';if(line[j]!='\\')esc=false;}out<<stringc<<line.substr(i,j-i)<<reset;i=j;continue;}
        if(std::isdigit(static_cast<unsigned char>(line[i]))){std::size_t j=i+1;while(j<line.size()&&(std::isalnum(static_cast<unsigned char>(line[j]))||line[j]=='.'||line[j]=='_'))++j;out<<number<<line.substr(i,j-i)<<reset;i=j;continue;}
        if(std::isalpha(static_cast<unsigned char>(line[i]))||line[i]=='_'){std::size_t j=i+1;while(j<line.size()&&is_ident(static_cast<unsigned char>(line[j])))++j;auto word=line.substr(i,j-i);if(is_kw(word))out<<keyword<<word<<reset;else out<<word;i=j;continue;}
        out<<line[i++];
    }
    return out.str();
}
}

std::string format_diagnostic_with_source(std::string_view path, std::string_view source, const Diagnostic& diagnostic, bool warning, bool color) {
    std::ostringstream out;
    out << (warning ? format_warning(path, diagnostic) : format_diagnostic(path, diagnostic));
    const auto line = source_line(source, diagnostic.span.begin.line);
    if (line.empty()) return out.str();
    out << '\n' << "  " << diagnostic.span.begin.line << " | " << highlight_line(line, color) << '\n' << "    | ";
    const std::size_t column = diagnostic.span.begin.column > 0 ? diagnostic.span.begin.column : 1;
    for (std::size_t i=1;i<column;++i) out << (i-1<line.size() && line[i-1]=='\t' ? '\t' : ' ');
    if (color) out << (warning ? "\x1b[1;33m" : "\x1b[1;31m");
    out << '^';
    std::size_t width = 1;
    if (diagnostic.span.end.line == diagnostic.span.begin.line && diagnostic.span.end.column > diagnostic.span.begin.column) width = diagnostic.span.end.column - diagnostic.span.begin.column;
    for (std::size_t i=1;i<width;++i) out << '~';
    if (color) out << "\x1b[0m";
    return out.str();
}
} // namespace strut
