#pragma once
#include <string_view>
#include <vector>
namespace strut {
enum class OperatorFixity { prefix, infix, postfix };
struct OperatorInfo { std::string_view spelling; OperatorFixity fixity; int precedence; bool overloadable; };
const std::vector<OperatorInfo>& operator_table();
const OperatorInfo* find_operator(std::string_view spelling, OperatorFixity fixity);
int infix_precedence(std::string_view spelling);
bool overloadable_operator(std::string_view spelling, OperatorFixity fixity);
}
