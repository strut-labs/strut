#include "strut/parser.h"
#include "strut/operator.h"

#include <array>
#include <charconv>
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
bool Parser::match_type_close() {
    if (pending_type_closers_ > 0) { --pending_type_closers_; return true; }
    if (check(">")) { advance(); return true; }
    if (check(">>")) { advance(); pending_type_closers_ = 1; return true; }
    return false;
}
void Parser::error(ParseResult& r,const Token&t,std::string m){r.diagnostics.push_back(Diagnostic{t.span,std::move(m)});}
void Parser::synchronize(){while(!at_end()){if(current_>0&&previous().lexeme==";")return;if(peek().lexeme=="const"||peek().kind==TokenKind::identifier)return;advance();}}

int Parser::precedence(std::string_view op) { return infix_precedence(op); }
bool Parser::is_binary_operator(std::string_view op){return precedence(op)>=0;}
bool Parser::is_assignment_operator(std::string_view op){return op=="="||op=="+="||op=="-="||op=="*="||op=="/="||op=="%="||op=="<<="||op==">>=";}

ExprPtr Parser::parse_lambda(ParseResult& result, bool is_async) {
    const Token begin = peek();
    if (!match("(")) { error(result, peek(), "expected '(' to start lambda parameters"); return nullptr; }
    auto data = std::make_shared<LambdaData>(); data->is_async = is_async;
    if (!check(")")) {
        do {
            TypeSyntax type{"", peek().span, false}; std::string name;
            const bool typed_parameter = check("const") || check("function") ||
                (peek().kind == TokenKind::identifier && peek(1).lexeme != "," && peek(1).lexeme != ")");
            if (typed_parameter) {
                type = parse_type(result); if (type.name.empty()) return nullptr; name = advance().lexeme;
                if (previous().kind != TokenKind::identifier) { error(result, previous(), "expected lambda parameter name"); return nullptr; }
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
            if(match("(")){
                const Token open=previous();
                auto first=parse_expression(result); if(!first)return nullptr;
                if(match(",")){
                    auto e=std::make_unique<Expr>(); e->kind=Expr::Kind::tuple_literal; e->arguments.push_back(std::move(first));
                    if(!check(")")){do{auto item=parse_expression(result);if(!item)return nullptr;e->arguments.push_back(std::move(item));}while(match(","));}
                    if(!match(")")){error(result,peek(),"expected ')' after tuple literal");return nullptr;}
                    e->span=join(open.span,previous().span);return e;
                }
                if(!match(")")){error(result,peek(),"expected ')' after expression");return nullptr;}
                auto e=std::make_unique<Expr>();e->kind=Expr::Kind::grouping;e->span=join(open.span,previous().span);e->left=std::move(first);return e;
            }
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
        if(match("::")){if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected name after '::'");return nullptr;}const Token member=advance();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::member;n->text="::"+member.lexeme;n->span=join(expr->span,member.span);n->left=std::move(expr);expr=std::move(n);continue;}
        if(match("?.")){if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected member name after '?.'");return nullptr;}const Token member=advance();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::safe_member;n->text=member.lexeme;n->span=join(expr->span,member.span);n->left=std::move(expr);expr=std::move(n);continue;}
        if(match("->")){if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected member name after '->'");return nullptr;}const Token member=advance();auto n=std::make_unique<Expr>();n->kind=Expr::Kind::member;n->text="->"+member.lexeme;n->span=join(expr->span,member.span);n->left=std::move(expr);expr=std::move(n);continue;}
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
    if(match("await")){const Token op=previous();auto rhs=parse_unary(result);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::unary;e->text="await";e->span=join(op.span,rhs->span);e->right=std::move(rhs);return e;}
    if(check("!")||check("~")||check("-")||check("+")||check("*")||check("++")||check("--")){const Token op=advance();auto rhs=parse_unary(result);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::unary;e->text=op.lexeme;e->span=join(op.span,rhs->span);e->right=std::move(rhs);return e;}return parse_postfix(result);
}
ExprPtr Parser::parse_expression(ParseResult& result,int minp){
    auto lhs=parse_unary(result); if(!lhs)return nullptr;
    while(peek().kind==TokenKind::op&&is_binary_operator(peek().lexeme)&&precedence(peek().lexeme)>=minp){const Token op=advance();const int p=precedence(op.lexeme);auto rhs=parse_expression(result,p+1);if(!rhs)return nullptr;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::binary;e->text=op.lexeme;e->span=join(lhs->span,rhs->span);e->left=std::move(lhs);e->right=std::move(rhs);lhs=std::move(e);}return lhs;
}

StmtPtr Parser::parse_declaration_or_assignment(ParseResult& result){
    const Token begin=peek();bool is_const=match("const");if(at_end()){error(result,peek(),"expected declaration after 'const'");return nullptr;}
    std::optional<TypeSyntax> type;Token name;
    if(is_type_token(peek())){std::size_t i=1;if(peek(i).lexeme=="<"){int depth=0;do{if(peek(i).lexeme=="<")++depth;else if(peek(i).lexeme==">")--depth;else if(peek(i).lexeme==">>")depth-=2;++i;}while(depth>0&&peek(i).kind!=TokenKind::end_of_file);}while(peek(i).lexeme=="*"||peek(i).lexeme=="&"){++i;if(peek(i).lexeme=="const")++i;}if(peek(i).lexeme=="?")++i;while(peek(i).lexeme=="["){++i;if(peek(i).kind==TokenKind::integer_literal)++i;if(peek(i).lexeme!="]")break;++i;}if(peek(i).kind==TokenKind::identifier&&(peek(i+1).lexeme==":="||peek(i+1).lexeme=="("||peek(i+1).lexeme==";")){type=parse_type(result);if(type->name.empty())return nullptr;name=advance();}}
    if(name.lexeme.empty() && peek().kind==TokenKind::identifier&&(peek(1).lexeme==":="||is_assignment_operator(peek(1).lexeme))){name=advance();}
    if(type && name.lexeme.size() && check("(")){const Token open=advance();auto call=std::make_unique<Expr>();call->kind=Expr::Kind::call;auto callee=std::make_unique<Expr>();callee->kind=Expr::Kind::identifier;callee->text=type->name;callee->span=type->span;call->left=std::move(callee);if(!check(")")){do{auto arg=parse_expression(result);if(!arg)return nullptr;call->arguments.push_back(std::move(arg));}while(match(","));}if(!match(")")){error(result,peek(),"expected ')' after constructor arguments");return nullptr;}call->span=join(open.span,previous().span);if(!match(";")){error(result,peek(),"expected ';' after declaration");return nullptr;}auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::declaration;st->span=join(begin.span,previous().span);st->name=name.lexeme;st->op=":=";st->declared_type=std::move(type);st->is_const=is_const;st->value=std::move(call);return st;}
    if(type && name.lexeme.size() && match(";")){auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::declaration;st->span=join(begin.span,previous().span);st->name=name.lexeme;st->op=":=";st->declared_type=std::move(type);st->is_const=is_const;return st;}
    if(name.lexeme.empty()) { // expression statement
        if(is_const){error(result,begin,"'const' may only be used on a declaration");return nullptr;}
        auto expr=parse_expression(result);if(!expr)return nullptr;
        if(peek().kind==TokenKind::op && is_assignment_operator(peek().lexeme)){std::string aop=advance().lexeme;auto rhs=parse_expression(result);if(!rhs)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after assignment");return nullptr;}auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::assignment;st->span=join(begin.span,previous().span);st->op=aop;st->target=std::move(expr);st->value=std::move(rhs);return st;}
        if(!match(";")){error(result,peek(),"expected ';' after statement");return nullptr;}auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::expression;st->span=join(begin.span,previous().span);st->value=std::move(expr);return st;
    }
    const bool declaration=match(":=");std::string op=declaration?":":"";if(!declaration){if(!is_assignment_operator(peek().lexeme)){error(result,peek(),"expected ':=' or assignment operator");return nullptr;}op=advance().lexeme;}
    if(is_const&&!declaration){error(result,begin,"'const' may only be used on a declaration");return nullptr;}
    auto value=parse_expression(result);if(!value)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after statement");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=declaration?Stmt::Kind::declaration:Stmt::Kind::assignment;st->span=join(begin.span,previous().span);st->name=name.lexeme;st->op=declaration?":=":op;st->declared_type=std::move(type);st->is_const=is_const;st->value=std::move(value);return st;
}

TypeSyntax Parser::parse_type(ParseResult& result) {
    const Token begin = peek();
    const bool binding_const = match("const");
    const Token type_begin = peek();
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
        if(!match("(")){error(result,peek(),"expected '(' in function type");return TypeSyntax{"",begin.span,false};}
        text += "<(";bool first_parameter=true;
        while(!at_end()&&!check(")")){if(!first_parameter){if(!match(",")){error(result,peek(),"expected ',' in function parameter types");return TypeSyntax{"",begin.span,false};}text+=",";}auto parameter=parse_type(result);if(parameter.name.empty())return TypeSyntax{"",begin.span,false};text+=parameter.name;first_parameter=false;}
        if(!match(")")){error(result,peek(),"expected ')' in function type");return TypeSyntax{"",begin.span,false};}
        if(!match("->")){error(result,peek(),"expected '->' in function type");return TypeSyntax{"",begin.span,false};}
        auto result_type=parse_type(result);if(result_type.name.empty())return TypeSyntax{"",begin.span,false};text+=")->"+result_type.name;
        if(!match_type_close()){error(result,peek(),"unterminated function type");return TypeSyntax{"",begin.span,false};}text+=">";
        return TypeSyntax{text, SourceSpan{begin.span.begin, previous().span.end}, binding_const};
    }
    if (!is_type_token(type_begin)) { error(result, type_begin, "expected type"); return TypeSyntax{"", begin.span, false}; }
    advance();
    std::string text = type_begin.lexeme;
    SourceSpan span = SourceSpan{begin.span.begin, type_begin.span.end};
    bool raw_pointer_surface = false;
    if (type_begin.lexeme == "ptr" && match("<")) {
        auto inner = parse_type(result);
        if (inner.name.empty()) return TypeSyntax{"", begin.span, false};
        if (!match_type_close()) { error(result, peek(), "expected '>' after raw pointer type"); return TypeSyntax{"", begin.span, false}; }
        text = "raw_ptr<" + inner.name + ">";
        span.end = previous().span.end;
        raw_pointer_surface = true;
    } else if (match("<")) {
        text += "<";
        bool first = true;
        while (!at_end() && !check(">") && !check(">>") && pending_type_closers_ == 0) {
            if (!first) {
                if (!match(",")) { error(result, peek(), "expected ',' between generic type arguments"); return TypeSyntax{"", begin.span, false}; }
                text += ",";
            }
            auto argument = parse_type(result);
            if (argument.name.empty()) return TypeSyntax{"", begin.span, false};
            text += argument.name;
            first = false;
        }
        if (first) { error(result, peek(), "generic type requires at least one argument"); return TypeSyntax{"", begin.span, false}; }
        if (!match_type_close()) { error(result, peek(), "expected '>' after generic type arguments"); return TypeSyntax{"", begin.span, false}; }
        text += ">";
        span.end = previous().span.end;
    }
    if (!raw_pointer_surface) {
        bool saw_reference = false;
        while (check("*") || check("&")) {
            const bool pointer = check("*");
            if (pointer && saw_reference) { error(result, peek(), "pointers to references are not allowed"); return TypeSyntax{"", begin.span, false}; }
            advance(); span.end = previous().span.end;
            const bool const_referent = match("const");
            if (const_referent) span.end = previous().span.end;
            if (pointer) text = std::string("ptr<") + (const_referent ? "const " : "") + text + ">";
            else {
                if (saw_reference) { error(result, previous(), "references to references are not allowed"); return TypeSyntax{"", begin.span, false}; }
                text = std::string("ref<") + (const_referent ? "const " : "") + text + ">";
                saw_reference = true;
            }
        }
    }
    if (match("?")) { text += "?"; span.end = previous().span.end; }
    while (match("[")) {
        text += "[";
        if (!check("]")) { if (peek().kind != TokenKind::integer_literal) { error(result, peek(), "expected array size or ']'"); return TypeSyntax{"", begin.span, false}; } text += advance().lexeme; }
        if (!match("]")) { error(result, peek(), "expected ']' in array type"); return TypeSyntax{"", begin.span, false}; }
        text += "]"; span.end = previous().span.end;
    }
    return TypeSyntax{text, span, binding_const};
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


StmtPtr Parser::parse_operator(ParseResult& result) {
    const Token begin=previous(); auto st=std::make_unique<Stmt>(); st->kind=Stmt::Kind::operator_decl;
    if(match("[")){
        if(check("]")){error(result,peek(),"generic operator list cannot be empty");return nullptr;}
        do{if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected generic parameter");return nullptr;}auto g=advance();bool upper=!g.lexeme.empty();for(unsigned char c:g.lexeme)if(std::islower(c))upper=false;if(!upper){error(result,g,"generic type parameters must use uppercase names");return nullptr;}st->generic_parameters.push_back(g.lexeme);}while(match(","));
        if(!match("]")){error(result,peek(),"expected ']' after operator generics");return nullptr;}
    }
    if(match("<")){
        if(!match("(")){error(result,peek(),"expected '(' in operator callable signature");return nullptr;}
        std::vector<TypeSyntax> types;
        if(!check(")")){do{auto t=parse_type(result);if(t.name.empty())return nullptr;types.push_back(std::move(t));}while(match(","));}
        if(!match(")")){error(result,peek(),"expected ')' in operator callable signature");return nullptr;}
        if(!match("->")){error(result,peek(),"expected '->' in operator callable signature");return nullptr;}
        st->return_type=parse_type(result);if(!st->return_type||st->return_type->name.empty())return nullptr;
        if(!match(">")){error(result,peek(),"expected '>' after operator callable signature");return nullptr;}
        if(peek().kind!=TokenKind::op){error(result,peek(),"expected overloadable operator token");return nullptr;}const Token op=advance();st->op=op.lexeme;
        const OperatorFixity fixity=types.size()==1?OperatorFixity::prefix:OperatorFixity::infix;
        if(!overloadable_operator(st->op,fixity)){error(result,op,"operator does not support this arity/fixity");return nullptr;}
        if(!match(":=")){error(result,peek(),"expected ':=' before operator lambda");return nullptr;}
        st->value=parse_expression(result);if(!st->value||st->value->kind!=Expr::Kind::lambda){error(result,peek(),"operator lambda declaration requires a lambda expression");return nullptr;}
        if(!match(";")){error(result,peek(),"expected ';' after operator lambda declaration");return nullptr;}
        if(!st->value->lambda || st->value->lambda->parameters.size()!=types.size()){error(result,op,"operator lambda parameter count does not match signature");return nullptr;}
        for(std::size_t i=0;i<types.size();++i){auto name=st->value->lambda->parameters[i].name;st->parameters.push_back(Parameter{std::move(types[i]),name,st->value->lambda->parameters[i].span});}
        st->has_body=true;st->span=join(begin.span,previous().span);return st;
    }
    if(peek().kind!=TokenKind::op){error(result,peek(),"expected overloadable operator after 'operator'");return nullptr;}
    const Token op=advance(); st->op=op.lexeme;
    if(!overloadable_operator(st->op,OperatorFixity::prefix) && !overloadable_operator(st->op,OperatorFixity::infix) && !overloadable_operator(st->op,OperatorFixity::postfix)){error(result,op,"operator is not overloadable in Strut");return nullptr;}
    if(!match("(")){error(result,peek(),"expected '(' after operator token");return nullptr;}
    if(!check(")")){do{auto type=parse_type(result);if(type.name.empty())return nullptr;if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected operator parameter name");return nullptr;}auto name=advance();st->parameters.push_back(Parameter{std::move(type),name.lexeme,name.span});}while(match(","));}
    if(!match(")")){error(result,peek(),"expected ')' after operator parameters");return nullptr;}
    if(!match("->")){error(result,peek(),"expected '->' and operator return type");return nullptr;}st->return_type=parse_type(result);if(!st->return_type||st->return_type->name.empty())return nullptr;
    const std::size_t arity=st->parameters.size();const OperatorFixity fixity=arity==1?OperatorFixity::prefix:OperatorFixity::infix;
    if(!overloadable_operator(st->op,fixity)){error(result,op,"operator does not support this arity/fixity");return nullptr;}
    if(match(";")){st->has_body=false;st->span=join(begin.span,previous().span);return st;}
    if(!match("{")){error(result,peek(),"expected operator body or ';'");return nullptr;}auto body=parse_block(result);if(!body)return nullptr;st->has_body=true;st->body=std::move(body->body);st->span=join(begin.span,body->span);return st;
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
    if(match(":")){
        if(match("(")){if(!check(")")){do{auto e=parse_type(result);if(e.name.empty())return nullptr;st->error_types.push_back(std::move(e));}while(match(","));}if(!match(")")){error(result,peek(),"expected ')' after error type list");return nullptr;}}
        else {auto e=parse_type(result);if(e.name.empty())return nullptr;st->error_types.push_back(std::move(e));}
    }
    if (match(";")) { st->has_body = false; st->span = join(begin.span, previous().span); return st; }
    if (!match("{")) { error(result, peek(), "expected function body or ';'"); return nullptr; }
    auto body = parse_block(result); if (!body) return nullptr;
    st->has_body = true; st->body = std::move(body->body); st->span = join(begin.span, body->span); return st;
}
StmtPtr Parser::parse_include(ParseResult& result) {
    const Token begin = previous();
    auto st = std::make_unique<Stmt>(); st->kind = Stmt::Kind::include_stmt;
    if (peek().kind == TokenKind::string_literal) {
        std::string value = advance().lexeme;
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size()-2);
        st->name = value; st->include_is_package = false;
    } else if (match("<")) {
        std::string value;
        while (!at_end() && !check(">")) value += advance().lexeme;
        if (!match(">")) { error(result, peek(), "expected '>' after package include"); return nullptr; }
        if (value.empty()) { error(result, previous(), "package include may not be empty"); return nullptr; }
        st->name = value; st->include_is_package = true;
    } else { error(result, peek(), "expected local string or <package> after include"); return nullptr; }
    match(";"); st->span = join(begin.span, previous().span); return st;
}

StmtPtr Parser::parse_enum(ParseResult& result) {
    const Token begin=previous(); if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected enum name");return nullptr;}
    const Token name=advance(); if(!match("{")){error(result,peek(),"expected '{' after enum name");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::enum_decl;st->name=name.lexeme;
    long long next=0;
    while(!at_end()&&!check("}")){
        if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected enum member name");return nullptr;}
        st->enum_names.push_back(advance().lexeme);
        if(match("=")){bool neg=match("-");if(peek().kind!=TokenKind::integer_literal){error(result,peek(),"enum values must be integer literals");return nullptr;}std::string v=(neg?"-":"")+advance().lexeme;long long parsed=0;auto conv=std::from_chars(v.data(),v.data()+v.size(),parsed);if(conv.ec!=std::errc{}||conv.ptr!=v.data()+v.size()){error(result,previous(),"enum integer value is out of range");return nullptr;}st->enum_values.push_back(v);next=parsed+1;}
        else {st->enum_values.push_back(std::to_string(next++));}
        if(check("}")) break;
        if(!match(",")){error(result,peek(),"expected ',' between enum members");return nullptr;}
    }
    if(!match("}")){error(result,peek(),"expected '}' after enum");return nullptr;}match(";");st->span=join(begin.span,previous().span);return st;
}

StmtPtr Parser::parse_struct(ParseResult& result) {
    const Token begin=previous();
    if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected struct name");return nullptr;}
    const Token name=advance();
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::struct_decl;st->name=name.lexeme;
    if(match("[")){do{if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected uppercase generic parameter");return nullptr;}std::string g=advance().lexeme;bool uppercase=!g.empty();for(char c:g)if(c>='a'&&c<='z')uppercase=false;if(!uppercase){error(result,previous(),"generic parameter names must be uppercase");return nullptr;}st->generic_parameters.push_back(g);}while(match(","));if(!match("]")){error(result,peek(),"expected ']' after struct generic parameters");return nullptr;}}
    if(match(":")){do{auto base=parse_type(result);if(base.name.empty())return nullptr;st->bases.push_back(base.name);}while(match(","));}
    if(!match("{")){error(result,peek(),"expected '{' after struct name");return nullptr;}
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
StmtPtr Parser::parse_match(ParseResult& result){
    const Token begin=previous(); if(!match("(")){error(result,peek(),"expected '(' after match");return nullptr;}
    auto value=parse_expression(result);if(!value)return nullptr;if(!match(")")){error(result,peek(),"expected ')' after match expression");return nullptr;}
    if(!match("{")){error(result,peek(),"expected '{' after match expression");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::match_stmt;st->condition=std::move(value);bool wildcard=false;
    while(!at_end()&&!check("}")){
        SwitchCase c; const Token cb=peek();
        if(peek().kind==TokenKind::identifier&&peek().lexeme=="_"){advance();if(wildcard){error(result,previous(),"duplicate match wildcard");return nullptr;}wildcard=true;c.is_default=true;}
        else {c.value=parse_expression(result);if(!c.value)return nullptr;}
        if(!match("=>")){error(result,peek(),"expected '=>' after match pattern");return nullptr;}
        if(!match("{")){error(result,peek(),"match arms require a braced block");return nullptr;}
        auto block=parse_block(result);if(!block)return nullptr;c.body=std::move(block->body);c.span=join(cb.span,block->span);st->switch_cases.push_back(std::move(c));
        match(",");
    }
    if(!match("}")){error(result,peek(),"expected '}' after match");return nullptr;}st->span=join(begin.span,previous().span);return st;
}

StmtPtr Parser::parse_switch(ParseResult& result){
    const Token begin=previous(); if(!match("(")){error(result,peek(),"expected '(' after switch");return nullptr;}
    auto value=parse_expression(result);if(!value)return nullptr;if(!match(")")){error(result,peek(),"expected ')' after switch expression");return nullptr;}
    if(!match("{")){error(result,peek(),"expected '{' after switch expression");return nullptr;}
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::switch_stmt;st->condition=std::move(value);
    bool seen_default=false;
    while(!at_end()&&!check("}")){
        SwitchCase c; const Token cb=peek();
        if(match("case")){c.value=parse_expression(result);if(!c.value)return nullptr;if(!match(":")){error(result,peek(),"expected ':' after case value");return nullptr;}}
        else if(match("default")){if(seen_default){error(result,previous(),"duplicate default case");return nullptr;}seen_default=true;c.is_default=true;if(!match(":")){error(result,peek(),"expected ':' after default");return nullptr;}}
        else {error(result,peek(),"expected case or default in switch");return nullptr;}
        if(!match("{")){error(result,peek(),"switch cases require a braced block");return nullptr;}
        auto block=parse_block(result);if(!block)return nullptr;c.body=std::move(block->body);c.span=join(cb.span,block->span);st->switch_cases.push_back(std::move(c));
    }
    if(!match("}")){error(result,peek(),"expected '}' after switch");return nullptr;}st->span=join(begin.span,previous().span);return st;
}

StmtPtr Parser::parse_for(ParseResult& result){
    const Token begin=previous();if(!match("(")){error(result,peek(),"expected '(' after 'for'");return nullptr;}
    // range form: identifier ':' expression
    if(peek().kind==TokenKind::identifier&&peek(1).lexeme==":"){Token name=advance();advance();auto items=parse_expression(result);if(!items)return nullptr;if(!match(")")){error(result,peek(),"expected ')' after range for");return nullptr;}if(!match("{")){error(result,peek(),"expected '{' after range for");return nullptr;}auto body=parse_block(result);auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::range_for;st->name=name.lexeme;st->value=std::move(items);st->body=std::move(body->body);st->span=join(begin.span,body->span);return st;}
    if(peek().kind==TokenKind::identifier&&peek(1).lexeme=="in"){
        error(result,peek(1),"Strut range loops use `:` rather than `in`; write `for ("+peek().lexeme+" : collection)`");
        return nullptr;
    }
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::for_stmt;
    if(!check(";")){st->initializer=parse_declaration_or_assignment(result);if(!st->initializer)return nullptr;}else advance();
    if(!check(";")){st->condition=parse_expression(result);if(!st->condition)return nullptr;}if(!match(";")){error(result,peek(),"expected ';' after for condition");return nullptr;}
    if(!check(")")){st->increment=parse_expression(result);if(!st->increment)return nullptr;}if(!match(")")){error(result,peek(),"expected ')' after for clauses");return nullptr;}if(!match("{")){error(result,peek(),"expected '{' after for clauses");return nullptr;}auto body=parse_block(result);if(!body)return nullptr;st->body=std::move(body->body);st->span=join(begin.span,body->span);return st;
}

StmtPtr Parser::parse_try(ParseResult& result){
    const Token begin=previous();
    if(!match("{")){error(result,peek(),"expected '{' after try");return nullptr;}
    auto body=parse_block(result);if(!body)return nullptr;
    auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::try_stmt;st->body=std::move(body->body);
    bool any=false;
    while(match("catch")){
        any=true;CatchClause c;const Token cb=previous();
        if(match("(")){
            auto t=parse_type(result);if(t.name.empty())return nullptr;c.type=std::move(t);
            if(peek().kind!=TokenKind::identifier){error(result,peek(),"expected catch variable name");return nullptr;}
            c.name=advance().lexeme;
            if(!match(")")){error(result,peek(),"expected ')' after catch binding");return nullptr;}
        }
        if(!match("{")){error(result,peek(),"expected '{' after catch");return nullptr;}
        auto block=parse_block(result);if(!block)return nullptr;c.body=std::move(block->body);c.span=join(cb.span,block->span);st->catches.push_back(std::move(c));
    }
    if(!any){error(result,peek(),"try requires at least one catch");return nullptr;}
    st->span=join(begin.span,st->catches.back().span);return st;
}

StmtPtr Parser::parse_statement(ParseResult& result){
    if (match("include")) return parse_include(result);
    if (match("type")) return parse_type_alias(result);
    if (match("struct")) return parse_struct(result);
    if (match("enum")) return parse_enum(result);
    if (match("unsafe")) { const Token kw=previous(); if(!match("{")){error(result,peek(),"expected '{' after unsafe");return nullptr;} auto block=parse_block(result); if(!block)return nullptr; block->kind=Stmt::Kind::unsafe_stmt; block->span=join(kw.span,block->span); return block; }
    if (check("function") && (peek(1).lexeme == "[" || peek(1).lexeme == "<")) return parse_typed_function_value(result);
    if (match("async")) { if(!match("function")){error(result,previous(),"expected function after async");return nullptr;} auto st=parse_function(result); if(st)st->is_async=true; return st; }
    if (match("extern")) { const Token kw=previous(); if(peek().kind!=TokenKind::string_literal || peek().lexeme!="\"C\""){error(result,peek(),"extern currently requires \"C\"");return nullptr;} advance(); if(!match("function")){error(result,peek(),"expected function after extern \"C\"");return nullptr;} auto st=parse_function(result); if(st){st->is_extern_c=true;if(st->has_body)error(result,kw,"extern \"C\" functions must be declarations ending in ';'");} return st; }
    if (match("function")) return parse_function(result);
    if (match("operator")) return parse_operator(result);
    if (match("{")) return parse_block(result);
    if (match("if")) return parse_if(result);
    if (match("while")) return parse_while(result);
    if (match("switch")) return parse_switch(result);
    if (match("match")) return parse_match(result);
    if (match("for")) return parse_for(result);
    if (match("try")) return parse_try(result);
    if (match("throw")) { const Token kw=previous(); auto st=std::make_unique<Stmt>();st->kind=Stmt::Kind::throw_stmt;st->value=parse_expression(result);if(!st->value)return nullptr;if(!match(";")){error(result,peek(),"expected ';' after throw");return nullptr;}st->span=join(kw.span,previous().span);return st; }
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
