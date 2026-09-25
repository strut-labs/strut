#pragma once

#include <string>
#include <string_view>

#include "strut/source.h"

namespace strut {

enum class TokenKind {
    end_of_file,
    identifier,
    keyword,
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
