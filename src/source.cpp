#include "strut/source.h"

#include <fstream>

namespace strut {

SourceFile::SourceFile(std::filesystem::path path, std::string text)
    : path_(std::move(path)), text_(std::move(text)) {}

bool SourceFile::has_strut_extension(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    return ext == ".p" || ext == ".h";
}

std::optional<SourceFile> SourceFile::load(const std::filesystem::path& path, std::string& error) {
    if (!has_strut_extension(path)) {
        error = "expected a Strut .p or .h source file";
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        error = "unable to open source file";
        return std::nullopt;
    }
    const auto end = input.tellg();
    if (end < 0) {
        error = "unable to determine source file size";
        return std::nullopt;
    }
    std::string text(static_cast<std::size_t>(end), '\0');
    input.seekg(0, std::ios::beg);
    if (!text.empty() && !input.read(text.data(), static_cast<std::streamsize>(text.size()))) {
        error = "failed while reading source file";
        return std::nullopt;
    }
    return SourceFile(path, std::move(text));
}

}
