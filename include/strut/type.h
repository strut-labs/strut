#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <cstddef>

namespace strut {

enum class TypeKind { invalid, void_type, bool_type, string_type, json_type, null_type, signed_int, unsigned_int, floating, named };
struct TypeInfo {
    TypeKind kind = TypeKind::invalid;
    int bits = 0;
    std::string name;
    bool valid() const { return kind != TypeKind::invalid; }
    bool numeric() const { return kind == TypeKind::signed_int || kind == TypeKind::unsigned_int || kind == TypeKind::floating; }
};

TypeInfo builtin_type(std::string_view name);
TypeInfo infer_integer_literal(std::string_view text);
TypeInfo infer_floating_literal(std::string_view text);
bool integer_literal_fits(std::string_view text, const TypeInfo& destination);
bool can_implicitly_convert(const TypeInfo& from, const TypeInfo& to);
std::optional<std::size_t> array_extent(std::string_view type_name);
bool is_nullable_type(std::string_view type_name);
std::string strip_nullable(std::string_view type_name);

} // namespace strut
