#pragma once

#include <string>
#include <string_view>

#include "strut/source.h"

namespace strut {

enum class TokenKind {
    end_of_file,
    identifier,
    keyword,
    integer_literal,
    floating_literal,
    string_literal,
    boolean_literal,
    null_literal,
    punctuation,
    op,
};

struct Token {
    TokenKind kind = TokenKind::end_of_file;
    std::string lexeme;
    SourceSpan span;
};

std::string_view token_kind_name(TokenKind kind);

}
