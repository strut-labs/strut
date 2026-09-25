#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

int main() {
    constexpr std::int32_t iterations = 5000000;
    auto shared = std::make_shared<std::int32_t>(7);
    volatile std::int64_t sink = 0;
    auto start = std::chrono::steady_clock::now();
    for (std::int32_t i = 0; i < iterations; ++i) {
        auto copy = shared;
        sink += *copy;
    }
    auto shared_end = std::chrono::steady_clock::now();
    auto* raw = shared.get();
    for (std::int32_t i = 0; i < iterations; ++i) {
        auto* copy = raw;
        sink += *copy;
    }
    auto raw_end = std::chrono::steady_clock::now();
    auto shared_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(shared_end - start).count();
    auto raw_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(raw_end - shared_end).count();
    std::cout << "shared_ptr_copy_ns=" << shared_ns << "\nraw_ptr_copy_ns=" << raw_ns << "\nsink=" << sink << "\n";
}
