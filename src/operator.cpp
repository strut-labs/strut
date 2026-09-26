#include "strut/operator.h"
namespace strut {
const std::vector<OperatorInfo>& operator_table(){
    static const std::vector<OperatorInfo> ops={
        {"??",OperatorFixity::infix,0,false},{"||",OperatorFixity::infix,1,true},{"&&",OperatorFixity::infix,2,true},
        {"|",OperatorFixity::infix,3,true},{"^",OperatorFixity::infix,4,true},{"&",OperatorFixity::infix,5,true},
        {"==",OperatorFixity::infix,6,true},{"!=",OperatorFixity::infix,6,true},{"<",OperatorFixity::infix,7,true},{"<=",OperatorFixity::infix,7,true},{">",OperatorFixity::infix,7,true},{">=",OperatorFixity::infix,7,true},
        {"<<",OperatorFixity::infix,8,true},{">>",OperatorFixity::infix,8,true},{"+",OperatorFixity::infix,9,true},{"-",OperatorFixity::infix,9,true},{"*",OperatorFixity::infix,10,true},{"/",OperatorFixity::infix,10,true},{"%",OperatorFixity::infix,10,true},
        {"+",OperatorFixity::prefix,11,true},{"-",OperatorFixity::prefix,11,true},{"!",OperatorFixity::prefix,11,true},{"~",OperatorFixity::prefix,11,true},{"*",OperatorFixity::prefix,11,true},
        {"++",OperatorFixity::postfix,12,false},{"--",OperatorFixity::postfix,12,false},
        {"=",OperatorFixity::infix,-1,true},{":=",OperatorFixity::infix,-1,true},{"[]",OperatorFixity::postfix,13,false},{"()",OperatorFixity::postfix,13,false}
    };return ops;
}
const OperatorInfo* find_operator(std::string_view spelling,OperatorFixity fixity){for(const auto& op:operator_table())if(op.spelling==spelling&&op.fixity==fixity)return &op;return nullptr;}
int infix_precedence(std::string_view spelling){auto* op=find_operator(spelling,OperatorFixity::infix);return op?op->precedence:-1;}
bool overloadable_operator(std::string_view spelling,OperatorFixity fixity){auto* op=find_operator(spelling,fixity);return op&&op->overloadable;}
}
