#include "strut/token.h"

namespace strut {

std::string_view token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::end_of_file: return "eof";
        case TokenKind::identifier: return "identifier";
        case TokenKind::keyword: return "keyword";
        case TokenKind::punctuation: return "punctuation";
        case TokenKind::op: return "operator";
    }
    return "unknown";
}

}
