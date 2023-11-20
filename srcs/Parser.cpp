#include "Parser.h"

std::unordered_map<Lexer::Token, int> Parser::op_prec_map = {
        {Lexer::tok_inc,         500}, // ++
        {Lexer::tok_dec,         500}, // --
        {Lexer::tok_bitnot,      490}, // ~
        {Lexer::tok_not,         490}, // !

        {Lexer::tok_mul,         480}, // *
        {Lexer::tok_div,         480}, // /
        {Lexer::tok_mod,         480}, // %

        {Lexer::tok_add,         470}, // +
        {Lexer::tok_sub,         470}, // -

        {Lexer::tok_lshift,      460}, // <<
        {Lexer::tok_rshift,      460}, // >>

        {Lexer::tok_lt,          450}, // <
        {Lexer::tok_gt,          450}, // >
        {Lexer::tok_lteq,        450}, // <=
        {Lexer::tok_gteq,        450}, // >=

        {Lexer::tok_eq,          440}, // ==
        {Lexer::tok_ineq,        440}, // !=

        {Lexer::tok_bitand,      430}, // &

        {Lexer::tok_bitxor,      420}, // ^

        {Lexer::tok_bitor,       410}, // |

        {Lexer::tok_and,         400}, // &&

        {Lexer::tok_or,          390}, // ||

        {Lexer::tok_assn,        380}, // =
        {Lexer::tok_mul_assn,    380}, // *=
        {Lexer::tok_div_assn,    380}, // /=
        {Lexer::tok_mod_assn,    380}, // %=
        {Lexer::tok_add_assn,    380}, // +=
        {Lexer::tok_sub_assn,    380}, // -=
        {Lexer::tok_lshift_assn, 380}, // <<=
        {Lexer::tok_rshift_assn, 380}, // >>=
        {Lexer::tok_and_assn,    380}, // &=
        {Lexer::tok_or_assn,     380}, // |=
        {Lexer::tok_xor_assn,    380}, // ^=
};
