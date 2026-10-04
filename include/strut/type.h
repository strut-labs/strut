#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <cstddef>
#include <vector>
#include <utility>

namespace strut {

struct TypeId {
    std::uint64_t value = 0;
    constexpr explicit operator bool() const { return value != 0; }
    friend constexpr bool operator==(TypeId a, TypeId b) { return a.value == b.value; }
    friend constexpr bool operator!=(TypeId a, TypeId b) { return !(a == b); }
};

enum class TypeNodeKind {
    invalid, primitive, named, const_type, reference, safe_pointer, raw_pointer,
    weak_pointer, nullable, vector, fixed_array, tuple, generic, function
};

struct TypeNode {
    TypeNodeKind kind = TypeNodeKind::invalid;
    std::string name;
    std::vector<TypeId> children;
    std::vector<TypeId> error_types;
    std::size_t extent = 0;
};

TypeId intern_type(std::string_view spelling);
const TypeNode& type_node(TypeId id);
std::string type_spelling(TypeId id);
std::string type_debug(TypeId id);
bool type_is(TypeId id, TypeNodeKind kind);
TypeId type_element(TypeId id);
const std::vector<TypeId>& type_arguments(TypeId id);

enum class TypeKind { invalid, void_type, bool_type, string_type, json_type, null_type, signed_int, unsigned_int, floating, named };
struct TypeInfo {
    TypeKind kind = TypeKind::invalid;
    int bits = 0;
    std::string name;
    TypeId id;
    TypeInfo() = default;
    TypeInfo(TypeKind type_kind, int width, std::string spelling, TypeId type_id = {})
        : kind(type_kind), bits(width), name(std::move(spelling)), id(type_id ? type_id : intern_type(name)) {}
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
