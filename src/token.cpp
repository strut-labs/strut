#include "strut/token.h"

namespace strut {

std::string_view token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::end_of_file: return "eof";
        case TokenKind::identifier: return "identifier";
        case TokenKind::keyword: return "keyword";
        case TokenKind::integer_literal: return "integer";
        case TokenKind::floating_literal: return "floating";
        case TokenKind::string_literal: return "string";
        case TokenKind::boolean_literal: return "boolean";
        case TokenKind::null_literal: return "null";
        case TokenKind::punctuation: return "punctuation";
        case TokenKind::op: return "operator";
    }
    return "unknown";
}

}
