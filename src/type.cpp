#include "strut/type.h"

#include <charconv>
#include <limits>

namespace strut {

TypeInfo builtin_type(std::string_view n) {
    if (n == "int") n = "int_32";
    if (n == "uint") n = "uint_32";
    if (n == "double") n = "double_32";
    if (n == "void") return {TypeKind::void_type, 0, std::string(n)};
    if (n == "bool") return {TypeKind::bool_type, 0, std::string(n)};
    if (n == "string") return {TypeKind::string_type, 0, std::string(n)};
    if (n == "int_8") return {TypeKind::signed_int, 8, std::string(n)};
    if (n == "int_16") return {TypeKind::signed_int, 16, std::string(n)};
    if (n == "int_32") return {TypeKind::signed_int, 32, std::string(n)};
    if (n == "int_64") return {TypeKind::signed_int, 64, std::string(n)};
    if (n == "uint_8") return {TypeKind::unsigned_int, 8, std::string(n)};
    if (n == "uint_16") return {TypeKind::unsigned_int, 16, std::string(n)};
    if (n == "uint_32") return {TypeKind::unsigned_int, 32, std::string(n)};
    if (n == "uint_64") return {TypeKind::unsigned_int, 64, std::string(n)};
    if (n == "double_32") return {TypeKind::floating, 32, std::string(n)};
    if (n == "double_64") return {TypeKind::floating, 64, std::string(n)};
    return {};
}

static std::optional<std::uint64_t> parse_u64(std::string_view text) {
    std::uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return value;
}

TypeInfo infer_integer_literal(std::string_view text) {
    auto value = parse_u64(text);
    if (!value) return {};
    if (*value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) return builtin_type("int_32");
    if (*value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return builtin_type("int_64");
    return builtin_type("uint_64");
}

TypeInfo infer_floating_literal(std::string_view) { return builtin_type("double_32"); }

bool integer_literal_fits(std::string_view text, const TypeInfo& destination) {
    auto value = parse_u64(text);
    if (!value) return false;
    if (destination.kind == TypeKind::unsigned_int) {
        if (destination.bits == 64) return true;
        return *value < (std::uint64_t{1} << destination.bits);
    }
    if (destination.kind == TypeKind::signed_int) {
        if (destination.bits == 64) return *value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        return *value < (std::uint64_t{1} << (destination.bits - 1));
    }
    return false;
}

std::optional<std::size_t> array_extent(std::string_view name) {
    if (name.size() < 3 || name.back() != ']') return std::nullopt;
    const auto open = name.rfind('[');
    if (open == std::string_view::npos || open + 1 == name.size() - 1) return std::nullopt;
    std::size_t value = 0;
    const auto first = name.data() + open + 1;
    const auto last = name.data() + name.size() - 1;
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) return std::nullopt;
    return value;
}

bool can_implicitly_convert(const TypeInfo& from, const TypeInfo& to) {
    if (!from.valid() || !to.valid()) return false;
    if (from.kind == to.kind && from.bits <= to.bits) return true;
    if ((from.kind == TypeKind::signed_int || from.kind == TypeKind::unsigned_int) &&
        to.kind == TypeKind::floating && to.bits >= 32) return true;
    return false;
}

} // namespace strut
