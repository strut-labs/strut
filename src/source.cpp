#include "strut/source.h"

#include <fstream>
#include <sstream>

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

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "unable to open source file";
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        error = "failed while reading source file";
        return std::nullopt;
    }

    return SourceFile(path, buffer.str());
}

}
