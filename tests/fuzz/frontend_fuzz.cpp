#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "strut/lexer.h"
#include "strut/parser.h"
#include "strut/sema.h"
#include "strut/source.h"

static void exercise(const std::string& text) {
    strut::SourceFile source("<fuzz>", text);
    strut::Lexer lexer(source);
    auto lexed = lexer.lex();
    if (!lexed.ok()) return;
    strut::Parser parser(lexed.tokens);
    auto parsed = parser.parse();
    if (!parsed.ok()) return;
    strut::SemanticAnalyzer sema;
    (void)sema.analyze(parsed.program);
}

int main() {
    const std::vector<std::string> seeds = {
        "function main() -> void { return; }",
        "x := 1; y := x + 2;",
        "struct User { int id; string name; }",
        "function max[T](T a, T b) -> T { if (a > b) { return a; } return b; }",
        "unsafe { raw_ptr<int> p := null; }",
        "for (x : [1,2,3]) { print(x); }",
        "try { throw IOError(\"x\"); } catch (IOError e) { print(e); }"
    };
    for (const auto& seed : seeds) exercise(seed);

    std::mt19937_64 rng(0x5354525554ULL);
    static constexpr char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_[]{}()<>=:+-*/%!&|?.;,\\\"' \n\t";
    std::uniform_int_distribution<int> len_dist(0, 384);
    std::uniform_int_distribution<std::size_t> char_dist(0, sizeof(alphabet) - 2);
    for (int i = 0; i < 10000; ++i) {
        std::string input;
        input.reserve(static_cast<std::size_t>(len_dist(rng)));
        const int length = len_dist(rng);
        for (int j = 0; j < length; ++j) input.push_back(alphabet[char_dist(rng)]);
        exercise(input);
    }
    std::cout << "frontend fuzz smoke: 10000 malformed/mutated inputs completed\n";
    return 0;
}
