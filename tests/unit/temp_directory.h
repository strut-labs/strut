#pragma once

#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

class TestTempDirectory {
public:
    explicit TestTempDirectory(const char* prefix) {
        std::random_device random;
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                (std::string(prefix) + "-" + std::to_string(random()));
            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) return;
        }
        throw std::runtime_error("unable to create unique test directory");
    }
    ~TestTempDirectory() { std::error_code error; std::filesystem::remove_all(path_, error); }
    TestTempDirectory(const TestTempDirectory&) = delete;
    TestTempDirectory& operator=(const TestTempDirectory&) = delete;
    const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};
