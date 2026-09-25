#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace strut {

struct SourceLocation {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct SourceSpan {
    SourceLocation begin;
    SourceLocation end;
};

class SourceFile {
public:
    static std::optional<SourceFile> load(const std::filesystem::path& path, std::string& error);
    static bool has_strut_extension(const std::filesystem::path& path);

    SourceFile(std::filesystem::path path, std::string text);

    const std::filesystem::path& path() const { return path_; }
    const std::string& text() const { return text_; }

private:
    std::filesystem::path path_;
    std::string text_;
};

}
