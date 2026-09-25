#include "strut/parser.h"

#include <array>
#include <string>

namespace strut {
namespace {
bool is_type_token(const Token& token) {
    return token.kind == TokenKind::identifier || (token.kind == TokenKind::keyword && token.lexeme == "void");
}
SourceSpan join(const SourceSpan& a, const SourceSpan& b) { return SourceSpan{a.begin, b.end}; }
}

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}
const Token& Parser::peek(std::size_t lookahead) const { const auto i=current_+lookahead; return tokens_[i<tokens_.size()?i:tokens_.size()-1]; }
const Token& Parser::previous() const { return tokens_[current_-1]; }
bool Parser::at_end() const { return peek().kind == TokenKind::end_of_file; }
const Token& Parser::advance() { if (!at_end()) ++current_; return previous(); }
bool Parser::check(std::string_view x) const { return !at_end() && peek().lexeme==x; }
bool Parser::match(std::string_view x) { if(!check(x)) return false; advance(); return true; }
void Parser::error(ParseResult& r,const Token&t,std::string m){r.diagnostics.push_back(Diagnostic{t.span,std::move(m)});}
void Parser::synchronize(){while(!at_end()){if(current_>0&&previous().lexeme==";")return;if(peek().lexeme=="const"||peek().kind==TokenKind::identifier)return;advance();}}

int Parser::precedence(std::string_view op) {
    if (op=="||") return 1;
    if (op=="&&") return 2;
    if (op=="|") return 3;
    if (op=="^") return 4;
    if (op=="&") return 5;
    if (op=="=="||op=="!=") return 6;
    if (op=="<"||op=="<="||op==">"||op==">=") return 7;
    if (op=="<<"||op==">>") return 8;
    if (op=="+"||op=="-") return 9;
    if (op=="*"||op=="/"||op=="%") return 10;
    return -1;
}
bool Parser::is_binary_operator(std::string_view op){return precedence(op)>=0;}
bool Parser::is_assignment_operator(std::string_view op){return op=="="||op=="+="||op=="-="||op=="*="||op=="/="||op=="%="||op=="<<="||op==">>=";}

ExprPtr Parser::parse_primary(ParseResult& result) {
    const Token token=peek(); Expr::Kind kind;
    switch(token.kind){
        case TokenKind::identifier: kind=Expr::Kind::identifier; break;
        case TokenKind::integer_literal: kind=Expr::Kind::integer_literal; break;
        case TokenKind::floating_literal: kind=Expr::Kind::floating_literal; break;
        case TokenKind::string_literal: kind=Expr::Kind::string_literal; break;
        case TokenKind::boolean_literal: kind=Expr::Kind::boolean_literal; break;
        case TokenKind::null_literal: kind=Expr::Kind::null_literal; break;
        default:
            if(match("(")){auto inner=parse_expression(result);if(!match(")")){error(result,peek(),"expected ')' after expression");return nullptr;}auto e=std::make_unique<Expr>();e->kind=Expr::Kind::grouping;e->span=join(token.span,previous().span);e->left=std::move(inner);return e;}
            error(result,token,"expected expression");return nullptr;
    }
    advance(); auto e=std::make_unique<Expr>(); e->kind=kind;e->text=token.lexeme;e->span=token.span;return e;
}

ExprPtr Parser::parse_postfix(ParseResult& result){
    auto expr=parse_primary(result); if(!expr)return nullptr;
    while(true){
        if(match(".")){if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected member name after '.'");return nullptr;}const Token member=advance();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::member;n->text=member.lexeme;n->span=join(expr->span,member.span);n->left=std::move(expr);expr=std::move(n);continue;}
        if(match("[")){auto idx=parse_expression(result);if(!idx)return nullptr;if(!match("]")){error(result,peek(),"expected ']' after index");return nullptr;}auto n=std::make_unique<Expr>();n->kind=Expr::Kind::index;n->span=join(expr->span,previous().span);n->left=std::move(expr);n->right=std::move(idx);expr=std::move(n);continue;}
        if(match("++")||match("--")){const Token op=previous();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::postfix;n->text=op.lexeme;n->span=join(expr->span,op.span);n->left=std::move(expr);expr=std::move(n);continue;}
        break;
    }
    return expr;
}
ExprPtr Parser::parse_unary(ParseResult& result){
    if(check("!")||check("~")||check("-")||check("+")||check("*")||check("++")||check("--")){const Token op=advance();auto rhs=parse_unary(result);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::unary;e->text=op.lexeme;e->span=join(op.span,rhs->span);e->right=std::move(rhs);return e;}return parse_postfix(result);
}
ExprPtr Parser::parse_expression(ParseResult& result,int minp){
    auto lhs=parse_unary(result); if(!lhs)return nullptr;
    while(peek().kind==TokenKind::op&&is_binary_operator(peek().lexeme)&&precedence(peek().lexeme)>=minp){const Token op=advance();const int p=precedence(op.lexeme);auto rhs=parse_expression(result,p+1);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::binary;e->text=op.lexeme;e->span=join(lhs->span,rhs->span);e->left=std::move(lhs);e->right=std::move(rhs);lhs=std::move(e);}return lhs;
}

StmtPtr Parser::parse_declaration_or_assignment(ParseResult& result){
    const Token begin=peek();bool is_const=match("const");if(at_end()){error(result,peek(),"expected declaration after 'const'");return nullptr;}
    std::optional<TypeSyntax> type;Token name;
    if(is_type_token(peek())&&peek(1).kind==TokenKind::identifier&&peek(2).lexeme==":="){const Token t=advance();type=TypeSyntax{t.lexeme,t.span,false};name=advance();}
    else if(peek().kind==TokenKind::identifier&&(peek(1).lexeme==":="||is_assignment_operator(peek(1).lexeme))){name=advance();}
    else { // expression statement
        if(is_const){error(result,begin,"'const' may only be used on a declaration");return nullptr;}
        auto expr=parse_expression(result);if(!expr)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after statement");return nullptr;}auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::expression;st->span=join(begin.span,previous().span);st->value=std::move(expr);return st;
    }
    const bool declaration=match(":=");std::string op=declaration?":":"";if(!declaration){if(!is_assignment_operator(peek().lexeme)){error(result,peek(),"expected ':=' or assignment operator");return nullptr;}op=advance().lexeme;}
    if(is_const&&!declaration){error(result,begin,"'const' may only be used on a declaration");return nullptr;}
    auto value=parse_expression(result);if(!value)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after statement");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=declaration?Stmt::Kind::declaration:Stmt::Kind::assignment;st->span=join(begin.span,previous().span);st->name=name.lexeme;st->op=declaration?":=":op;st->declared_type=std::move(type);st->is_const=is_const;st->value=std::move(value);return st;
}
StmtPtr Parser::parse_statement(ParseResult& result){return parse_declaration_or_assignment(result);}
ParseResult Parser::parse(){ParseResult r;while(!at_end()){const auto before=current_;auto st=parse_statement(r);if(st)r.program.statements.push_back(std::move(st));if(current_==before)advance();if(!r.diagnostics.empty())synchronize();}return r;}

} // namespace strut
