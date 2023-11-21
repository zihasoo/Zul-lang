#include "Parser.h"

std::unordered_map<Token, int> Parser::op_prec_map = {
        {tok_inc,         500}, // ++
        {tok_dec,         500}, // --
        {tok_bitnot,      490}, // ~
        {tok_not,         490}, // !

        {tok_mul,         480}, // *
        {tok_div,         480}, // /
        {tok_mod,         480}, // %

        {tok_add,         470}, // +
        {tok_sub,         470}, // -

        {tok_lshift,      460}, // <<
        {tok_rshift,      460}, // >>

        {tok_lt,          450}, // <
        {tok_gt,          450}, // >
        {tok_lteq,        450}, // <=
        {tok_gteq,        450}, // >=

        {tok_eq,          440}, // ==
        {tok_ineq,        440}, // !=

        {tok_bitand,      430}, // &

        {tok_bitxor,      420}, // ^

        {tok_bitor,       410}, // |

        {tok_and,         400}, // &&

        {tok_or,          390}, // ||

        {tok_assn,        380}, // =
        {tok_mul_assn,    380}, // *=
        {tok_div_assn,    380}, // /=
        {tok_mod_assn,    380}, // %=
        {tok_add_assn,    380}, // +=
        {tok_sub_assn,    380}, // -=
        {tok_lshift_assn, 380}, // <<=
        {tok_rshift_assn, 380}, // >>=
        {tok_and_assn,    380}, // &=
        {tok_or_assn,     380}, // |=
        {tok_xor_assn,    380}, // ^=
};
