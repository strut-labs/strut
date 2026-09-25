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
    if (op=="??") return 0;
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

ExprPtr Parser::parse_lambda(ParseResult& result, bool is_async) {
    const Token begin = peek();
    if (!match("(")) { error(result, peek(), "expected '(' to start lambda parameters"); return nullptr; }
    auto data = std::make_shared<LambdaData>(); data->is_async = is_async;
    if (!check(")")) {
        do {
            TypeSyntax type{"", peek().span, false}; std::string name;
            if (peek().kind == TokenKind::identifier && peek(1).kind == TokenKind::identifier) {
                type = parse_type(result); if (type.name.empty()) return nullptr; name = advance().lexeme;
                bool upper = !type.name.empty(); for(char c:type.name) if(c>='a'&&c<='z') upper=false;
                if (upper) data->generic_parameters.push_back(type.name);
            } else if (peek().kind == TokenKind::identifier) {
                name = advance().lexeme;
            } else { error(result, peek(), "expected lambda parameter"); return nullptr; }
            data->parameters.push_back(Parameter{std::move(type), name, previous().span});
        } while (match(","));
    }
    if (!match(")")) { error(result, peek(), "expected ')' after lambda parameters"); return nullptr; }
    if (!match("=>")) { error(result, peek(), "expected '=>' after lambda parameters"); return nullptr; }
    auto expr = std::make_unique<Expr>(); expr->kind = Expr::Kind::lambda; expr->lambda = data;
    if (match("{")) { auto block = parse_block(result); if (!block) return nullptr; data->body = std::move(block->body); expr->span = SourceSpan{begin.span.begin, block->span.end}; }
    else { data->expression_body = parse_expression(result); if (!data->expression_body) return nullptr; expr->span = SourceSpan{begin.span.begin, data->expression_body->span.end}; }
    return expr;
}

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
            if (match("{")) {
                const Token open=previous();auto e=std::make_unique<Expr>();e->kind=Expr::Kind::json_object;
                if(!check("}")){do{if(peek().kind!=TokenKind::string_literal){error(result,peek(),"JSON object keys must be string literals");return nullptr;}const Token key=advance();auto k=std::make_unique<Expr>();k->kind=Expr::Kind::string_literal;k->text=key.lexeme;k->span=key.span;e->arguments.push_back(std::move(k));if(!match(":")){error(result,peek(),"expected ':' after JSON object key");return nullptr;}auto value=parse_expression(result);if(!value)return nullptr;e->arguments.push_back(std::move(value));}while(match(","));}
                if(!match("}")){error(result,peek(),"expected '}' after JSON object");return nullptr;}e->span=join(open.span,previous().span);return e;
            }
            if (match("[")) {
                const Token open=previous();
                auto e=std::make_unique<Expr>();
                if(check("]")){e->kind=Expr::Kind::array_literal;advance();e->span=join(open.span,previous().span);return e;}
                auto first=parse_expression(result);if(!first)return nullptr;
                if(match(":")){
                    e->kind=Expr::Kind::map_literal;e->arguments.push_back(std::move(first));
                    auto value=parse_expression(result);if(!value)return nullptr;e->arguments.push_back(std::move(value));
                    while(match(",")){auto key=parse_expression(result);if(!key)return nullptr;if(!match(":")){error(result,peek(),"expected ':' in map literal");return nullptr;}auto val=parse_expression(result);if(!val)return nullptr;e->arguments.push_back(std::move(key));e->arguments.push_back(std::move(val));}
                } else {
                    e->kind=Expr::Kind::array_literal;e->arguments.push_back(std::move(first));
                    while(match(",")){auto item=parse_expression(result);if(!item)return nullptr;e->arguments.push_back(std::move(item));}
                }
                if(!match("]")){error(result,peek(),"expected ']' after collection literal");return nullptr;}e->span=join(open.span,previous().span);return e;
            }
            if(match("(")){auto inner=parse_expression(result);if(!match(")")){error(result,peek(),"expected ')' after expression");return nullptr;}auto e=std::make_unique<Expr>();e->kind=Expr::Kind::grouping;e->span=join(token.span,previous().span);e->left=std::move(inner);return e;}
            error(result,token,"expected expression");return nullptr;
    }
    advance(); auto e=std::make_unique<Expr>(); e->kind=kind;e->text=token.lexeme;e->span=token.span;return e;
}

ExprPtr Parser::parse_postfix(ParseResult& result){
    auto expr=parse_primary(result); if(!expr)return nullptr;
    while(true){
        if(expr->kind==Expr::Kind::identifier && match("{")){
            auto n=std::make_unique<Expr>();n->kind=Expr::Kind::struct_literal;n->text=expr->text;const SourceSpan begin=expr->span;
            if(!check("}")){do{if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected struct field name");return nullptr;}n->names.push_back(advance().lexeme);if(!match(":")){error(result,peek(),"expected ':' after struct field name");return nullptr;}auto value=parse_expression(result);if(!value)return nullptr;n->arguments.push_back(std::move(value));}while(match(","));}
            if(!match("}")){error(result,peek(),"expected '}' after struct literal");return nullptr;}n->span=SourceSpan{begin.begin,previous().span.end};expr=std::move(n);continue;
        }
        if(match("(")){
            auto n=std::make_unique<Expr>();n->kind=Expr::Kind::call;n->left=std::move(expr);
            if(!check(")")){do{auto arg=parse_expression(result);if(!arg)return nullptr;n->arguments.push_back(std::move(arg));}while(match(","));}
            if(!match(")")){error(result,peek(),"expected ')' after call arguments");return nullptr;}
            n->span=join(n->left->span,previous().span);expr=std::move(n);continue;
        }
        if(match("?.")){if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected member name after '?.'");return nullptr;}const Token member=advance();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::safe_member;n->text=member.lexeme;n->span=join(expr->span,member.span);n->left=std::move(expr);expr=std::move(n);continue;}
        if(match(".")){if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected member name after '.'");return nullptr;}const Token member=advance();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::member;n->text=member.lexeme;n->span=join(expr->span,member.span);n->left=std::move(expr);expr=std::move(n);continue;}
        if(match("[")){auto idx=parse_expression(result);if(!idx)return nullptr;if(!match("]")){error(result,peek(),"expected ']' after index");return nullptr;}auto n=std::make_unique<Expr>();n->kind=Expr::Kind::index;n->span=join(expr->span,previous().span);n->left=std::move(expr);n->right=std::move(idx);expr=std::move(n);continue;}
        if(match("++")||match("--")){const Token op=previous();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::postfix;n->text=op.lexeme;n->span=join(expr->span,op.span);n->left=std::move(expr);expr=std::move(n);continue;}
        break;
    }
    return expr;
}
ExprPtr Parser::parse_unary(ParseResult& result){
    if (match("async")) return parse_lambda(result, true);
    if (check("(")) { std::size_t i=current_, depth=0; bool lambda=false; for(;i<tokens_.size();++i){if(tokens_[i].lexeme=="(")++depth;else if(tokens_[i].lexeme==")"){if(--depth==0){lambda=(i+1<tokens_.size()&&tokens_[i+1].lexeme=="=>");break;}}} if(lambda) return parse_lambda(result,false); }
    if(check("!")||check("~")||check("-")||check("+")||check("*")||check("++")||check("--")){const Token op=advance();auto rhs=parse_unary(result);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::unary;e->text=op.lexeme;e->span=join(op.span,rhs->span);e->right=std::move(rhs);return e;}return parse_postfix(result);
}
ExprPtr Parser::parse_expression(ParseResult& result,int minp){
    auto lhs=parse_unary(result); if(!lhs)return nullptr;
    while(peek().kind==TokenKind::op&&is_binary_operator(peek().lexeme)&&precedence(peek().lexeme)>=minp){const Token op=advance();const int p=precedence(op.lexeme);auto rhs=parse_expression(result,p+1);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::binary;e->text=op.lexeme;e->span=join(lhs->span,rhs->span);e->left=std::move(lhs);e->right=std::move(rhs);lhs=std::move(e);}return lhs;
}

StmtPtr Parser::parse_declaration_or_assignment(ParseResult& result){
    const Token begin=peek();bool is_const=match("const");if(at_end()){error(result,peek(),"expected declaration after 'const'");return nullptr;}
    std::optional<TypeSyntax> type;Token name;
    if(is_type_token(peek())){std::size_t i=1;if(peek(i).lexeme=="<"){int depth=0;do{if(peek(i).lexeme=="<")++depth;else if(peek(i).lexeme==">")--depth;++i;}while(depth>0&&peek(i).kind!=TokenKind::end_of_file);}if(peek(i).lexeme=="?")++i;while(peek(i).lexeme=="["){++i;if(peek(i).kind==TokenKind::integer_literal)++i;if(peek(i).lexeme!="]")break;++i;}if(peek(i).kind==TokenKind::identifier&&peek(i+1).lexeme==":="){type=parse_type(result);if(type->name.empty())return nullptr;name=advance();}}
    if(name.lexeme.empty() && peek().kind==TokenKind::identifier&&(peek(1).lexeme==":="||is_assignment_operator(peek(1).lexeme))){name=advance();}
    if(name.lexeme.empty()) { // expression statement
        if(is_const){error(result,begin,"'const' may only be used on a declaration");return nullptr;}
        auto expr=parse_expression(result);if(!expr)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after statement");return nullptr;}auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::expression;st->span=join(begin.span,previous().span);st->value=std::move(expr);return st;
    }
    const bool declaration=match(":=");std::string op=declaration?":":"";if(!declaration){if(!is_assignment_operator(peek().lexeme)){error(result,peek(),"expected ':=' or assignment operator");return nullptr;}op=advance().lexeme;}
    if(is_const&&!declaration){error(result,begin,"'const' may only be used on a declaration");return nullptr;}
    auto value=parse_expression(result);if(!value)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after statement");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=declaration?Stmt::Kind::declaration:Stmt::Kind::assignment;st->span=join(begin.span,previous().span);st->name=name.lexeme;st->op=declaration?":=":op;st->declared_type=std::move(type);st->is_const=is_const;st->value=std::move(value);return st;
}

TypeSyntax Parser::parse_type(ParseResult& result) {
    const Token begin = peek();
    if (check("function")) {
        std::string text;
        text += advance().lexeme;
        if (match("[")) {
            text += "[";
            bool first = true;
            while (!at_end() && !check("]")) {
                if (!first) { if (!match(",")) { error(result, peek(), "expected ',' in function generic list"); return TypeSyntax{"", begin.span, false}; } text += ","; }
                if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected uppercase generic parameter"); return TypeSyntax{"", begin.span, false}; }
                const std::string g = advance().lexeme; for(char c:g) if(c>='a'&&c<='z'){error(result,previous(),"generic parameter names must be uppercase");return TypeSyntax{"",begin.span,false};}
                text += g; first = false;
            }
            if (!match("]")) { error(result, peek(), "expected ']' after function generics"); return TypeSyntax{"", begin.span, false}; }
            text += "]";
        }
        if (!match("<")) { error(result, peek(), "expected '<' in function type"); return TypeSyntax{"", begin.span, false}; }
        text += "<";
        int depth = 1;
        while (!at_end() && depth > 0) {
            if (check("<")) { ++depth; text += advance().lexeme; continue; }
            if (check(">")) { --depth; text += advance().lexeme; continue; }
            text += advance().lexeme;
        }
        if (depth != 0) { error(result, peek(), "unterminated function type"); return TypeSyntax{"", begin.span, false}; }
        return TypeSyntax{text, SourceSpan{begin.span.begin, previous().span.end}, false};
    }
    if (!is_type_token(begin)) { error(result, begin, "expected type"); return TypeSyntax{"", begin.span, false}; }
    advance();
    std::string text = begin.lexeme;
    SourceSpan span = begin.span;
    if (match("<")) {
        text += "<"; int depth = 1;
        while (!at_end() && depth > 0) {
            if (check("<")) { ++depth; text += advance().lexeme; continue; }
            if (check(">")) { --depth; text += advance().lexeme; span.end = previous().span.end; continue; }
            text += advance().lexeme;
        }
        if (depth != 0) { error(result, peek(), "unterminated generic type"); return TypeSyntax{"", begin.span, false}; }
    }
    if (match("?")) { text += "?"; span.end = previous().span.end; }
    while (match("[")) {
        text += "[";
        if (!check("]")) { if (peek().kind != TokenKind::integer_literal) { error(result, peek(), "expected array size or ']'"); return TypeSyntax{"", begin.span, false}; } text += advance().lexeme; }
        if (!match("]")) { error(result, peek(), "expected ']' in array type"); return TypeSyntax{"", begin.span, false}; }
        text += "]"; span.end = previous().span.end;
    }
    return TypeSyntax{text, span, false};
}

StmtPtr Parser::parse_type_alias(ParseResult& result) {
    const Token begin = previous();
    if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected alias name after 'type'"); return nullptr; }
    Token name = advance();
    if (!match(":=")) { error(result, peek(), "expected ':=' in type alias"); return nullptr; }
    auto target = parse_type(result); if (target.name.empty()) return nullptr;
    if (!match(";")) { error(result, peek(), "expected ';' after type alias"); return nullptr; }
    auto st=std::make_unique<Stmt>(); st->kind=Stmt::Kind::type_alias; st->name=name.lexeme; st->alias_target=std::move(target); st->span=join(begin.span,previous().span); return st;
}

StmtPtr Parser::parse_typed_function_value(ParseResult& result) {
    const Token begin = peek();
    auto type = parse_type(result); if (type.name.empty()) return nullptr;
    if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected variable name after function type"); return nullptr; }
    Token name = advance();
    if (!match(":=")) { error(result, peek(), "expected ':=' after function-valued variable name"); return nullptr; }
    auto value = parse_expression(result); if (!value) return nullptr;
    if (!match(";")) { error(result, peek(), "expected ';' after function-valued declaration"); return nullptr; }
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::declaration;st->name=name.lexeme;st->declared_type=std::move(type);st->value=std::move(value);st->span=join(begin.span,previous().span);return st;
}

StmtPtr Parser::parse_function(ParseResult& result) {
    const Token begin = previous();
    if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected function name"); return nullptr; }
    Token first = advance();
    std::string owner;
    std::string name = first.lexeme;
    if (match("::")) {
        owner = first.lexeme;
        if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected method name after '::'"); return nullptr; }
        name = advance().lexeme;
    }
    auto st = std::make_unique<Stmt>();
    st->kind = Stmt::Kind::function_decl;
    st->name = name;
    st->owner = owner;
    if (match("[")) {
        do {
            if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected uppercase generic parameter"); return nullptr; }
            const std::string generic = advance().lexeme;
            bool uppercase = !generic.empty();
            for (const char c : generic) if (c >= 'a' && c <= 'z') uppercase = false;
            if (!uppercase) { error(result, previous(), "generic parameter names must be uppercase"); return nullptr; }
            st->generic_parameters.push_back(generic);
        } while (match(","));
        if (!match("]")) { error(result, peek(), "expected ']' after generic parameters"); return nullptr; }
    }
    if (!match("(")) { error(result, peek(), "expected '(' after function name"); return nullptr; }
    if (!check(")")) {
        do {
            auto type = parse_type(result); if (type.name.empty()) return nullptr;
            if (peek().kind != TokenKind::identifier) { error(result, peek(), "expected parameter name"); return nullptr; }
            Token param_name = advance();
            st->parameters.push_back(Parameter{std::move(type), param_name.lexeme, param_name.span});
        } while (match(","));
    }
    if (!match(")")) { error(result, peek(), "expected ')' after parameters"); return nullptr; }
    if (!match("->")) { error(result, peek(), "expected '->' and explicit return type"); return nullptr; }
    auto return_type = parse_type(result); if (return_type.name.empty()) return nullptr;
    st->return_type = std::move(return_type);
    if (match(";")) { st->has_body = false; st->span = join(begin.span, previous().span); return st; }
    if (!match("{")) { error(result, peek(), "expected function body or ';'"); return nullptr; }
    auto body = parse_block(result); if (!body) return nullptr;
    st->has_body = true; st->body = std::move(body->body); st->span = join(begin.span, body->span); return st;
}
StmtPtr Parser::parse_struct(ParseResult& result) {
    const Token begin=previous();
    if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected struct name");return nullptr;}
    const Token name=advance();
    if(!match("{")){error(result,peek(),"expected '{' after struct name");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::struct_decl;st->name=name.lexeme;
    while(!at_end()&&!check("}")){
        if(match("function")){
            auto method=parse_function(result);if(!method)return nullptr;method->owner=st->name;st->body.push_back(std::move(method));continue;
        }
        auto type=parse_type(result);if(type.name.empty())return nullptr;
        if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected field name");return nullptr;}
        const Token field=advance();
        if(!match(";")){error(result,peek(),"expected ';' after struct field");return nullptr;}
        st->fields.push_back(Parameter{std::move(type),field.lexeme,field.span});
    }
    if(!match("}")){error(result,peek(),"expected '}' after struct");return nullptr;}
    if(match(";")){} // optional compatibility semicolon after a struct definition
    st->span=join(begin.span,previous().span);return st;
}

StmtPtr Parser::parse_block(ParseResult& result){
    const Token begin=previous();auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::block;
    while(!at_end()&&!check("}")){auto child=parse_statement(result);if(child)st->body.push_back(std::move(child));else if(!at_end())advance();}
    if(!match("}")){error(result,peek(),"expected '}' after block");return nullptr;}st->span=join(begin.span,previous().span);return st;
}
StmtPtr Parser::parse_if(ParseResult& result){
    const Token begin=previous();if(!match("(")){error(result,peek(),"expected '(' after 'if'");return nullptr;}auto cond=parse_expression(result);if(!cond)return nullptr;if(!match(")")){error(result,peek(),"expected ')' after if condition");return nullptr;}if(!match("{")){error(result,peek(),"expected '{' after if condition");return nullptr;}auto then_block=parse_block(result);if(!then_block)return nullptr;auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::if_stmt;st->condition=std::move(cond);st->body=std::move(then_block->body);st->span=join(begin.span,then_block->span);
    if(match("else")){if(match("if")){auto nested=parse_if(result);if(!nested)return nullptr;st->else_body.push_back(std::move(nested));st->span.end=st->else_body.back()->span.end;}else{if(!match("{")){error(result,peek(),"expected '{' after 'else'");return nullptr;}auto eb=parse_block(result);if(!eb)return nullptr;st->else_body=std::move(eb->body);st->span.end=eb->span.end;}}return st;
}
StmtPtr Parser::parse_while(ParseResult& result){const Token begin=previous();if(!match("(")){error(result,peek(),"expected '(' after 'while'");return nullptr;}auto cond=parse_expression(result);if(!cond)return nullptr;if(!match(")")){error(result,peek(),"expected ')' after while condition");return nullptr;}if(!match("{")){error(result,peek(),"expected '{' after while condition");return nullptr;}auto body=parse_block(result);if(!body)return nullptr;auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::while_stmt;st->condition=std::move(cond);st->body=std::move(body->body);st->span=join(begin.span,body->span);return st;}
StmtPtr Parser::parse_for(ParseResult& result){
    const Token begin=previous();if(!match("(")){error(result,peek(),"expected '(' after 'for'");return nullptr;}
    // range form: identifier ':' expression
    if(peek().kind==TokenKind::identifier&&peek(1).lexeme==":"){Token name=advance();advance();auto items=parse_expression(result);if(!items)return nullptr;if(!match(")")){error(result,peek(),"expected ')' after range for");return nullptr;}if(!match("{")){error(result,peek(),"expected '{' after range for");return nullptr;}auto body=parse_block(result);auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::range_for;st->name=name.lexeme;st->value=std::move(items);st->body=std::move(body->body);st->span=join(begin.span,body->span);return st;}
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::for_stmt;
    if(!check(";")){st->initializer=parse_declaration_or_assignment(result);if(!st->initializer)return nullptr;}else advance();
    if(!check(";")){st->condition=parse_expression(result);if(!st->condition)return nullptr;}if(!match(";")){error(result,peek(),"expected ';' after for condition");return nullptr;}
    if(!check(")")){st->increment=parse_expression(result);if(!st->increment)return nullptr;}if(!match(")")){error(result,peek(),"expected ')' after for clauses");return nullptr;}if(!match("{")){error(result,peek(),"expected '{' after for clauses");return nullptr;}auto body=parse_block(result);if(!body)return nullptr;st->body=std::move(body->body);st->span=join(begin.span,body->span);return st;
}
StmtPtr Parser::parse_statement(ParseResult& result){
    if (match("type")) return parse_type_alias(result);
    if (match("struct")) return parse_struct(result);
    if (check("function") && (peek(1).lexeme == "[" || peek(1).lexeme == "<")) return parse_typed_function_value(result);
    if (match("function")) return parse_function(result);
    if (match("{")) return parse_block(result);
    if (match("if")) return parse_if(result);
    if (match("while")) return parse_while(result);
    if (match("for")) return parse_for(result);
    if (match("return")) {
        const Token kw = previous();
        auto st = std::make_unique<Stmt>(); st->kind = Stmt::Kind::return_stmt;
        if (!check(";")) st->value = parse_expression(result);
        if (!match(";")) { error(result, peek(), "expected ';' after return"); return nullptr; }
        st->span = join(kw.span, previous().span); return st;
    }
    if (match("break") || match("continue")) {const Token kw=previous();if(!match(";")){error(result,peek(),"expected ';' after control statement");return nullptr;}auto st=std::make_unique<Stmt>();st->kind=kw.lexeme=="break"?Stmt::Kind::break_stmt:Stmt::Kind::continue_stmt;st->span=join(kw.span,previous().span);return st;}
    return parse_declaration_or_assignment(result);
}
ParseResult Parser::parse(){ParseResult r;while(!at_end()){const auto before=current_;auto st=parse_statement(r);if(st)r.program.statements.push_back(std::move(st));if(current_==before)advance();if(!r.diagnostics.empty())synchronize();}return r;}

} // namespace strut
