
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULLANG_LEXER_H
#define ZULLANG_LEXER_H

#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <initializer_list>

#include "Logger.h"
#include "System.h"
#include "Utility.h"

enum Token {
    // keyword
    tok_hi, //ㅎㅇ
    tok_go, //ㄱㄱ
    tok_ij, //ㅇㅈ?
    tok_no, //ㄴㄴ?
    tok_nope, //ㄴㄴ
    tok_gg, // ㅈㅈ
    tok_sg, //ㅅㄱ
    tok_tt, //ㅌㅌ
    tok_true, //참
    tok_false, //거짓

    // primary
    tok_identifier,
    tok_int,
    tok_real,
    tok_indent,
    tok_newline,

    // operators
    tok_comma, // ,
    tok_colon, // :
    tok_semicolon, // ;
    tok_lpar, // (
    tok_rpar, // )
    tok_rsqbrk, // [
    tok_lsqbrk, // ]
    tok_rbrk, // {
    tok_lbrk, // }
    tok_dot, // .
    tok_dquotes, //"
    tok_squotes, //'
    tok_anno, // //
    tok_va_arg, // ...

    tok_add, // +
    tok_sub, // -
    tok_mul, // *
    tok_div, // /
    tok_mod, // %

    tok_and, // &&
    tok_or, // ||
    tok_not, // !
    tok_bitand, // &
    tok_bitor, // |
    tok_bitnot, // ~
    tok_bitxor, // ^
    tok_lshift, // <<
    tok_rshift, // >>

    tok_assn, // =
    tok_mul_assn, // *=
    tok_div_assn, // /=
    tok_mod_assn, // %=
    tok_add_assn, // +=
    tok_sub_assn, // -=
    tok_lshift_assn, // <<=
    tok_rshift_assn, // >>=
    tok_and_assn, // &=
    tok_or_assn, // |=
    tok_xor_assn, // ^=

    tok_eq, // ==
    tok_ineq, // !=
    tok_gt,// >
    tok_gteq,// >=
    tok_lt, // <
    tok_lteq, // <=

    //eof
    tok_eof,

    //undefined
    tok_undefined
};

class Lexer {
public:
    explicit Lexer(const std::string& source_name);

    Token get_token();

    std::string& get_word();

    std::pair<int, int> get_token_loc();

    int get_line_index();

    std::string get_line_substr(int st, int ed);

    void log_unexpected(std::string_view msg = "");

    void log_token(std::string_view msg);

    void log_token(const std::initializer_list<std::string_view>& msgs);

private:
    std::string cur_line;

    int last_char = ' '; //마지막으로 읽은 글자 (유니코드)

    std::string last_word;

    std::string raw_last_char; //utf8 한 글자를 그대로 저장하기 위한 버퍼

    std::ifstream source;

    std::pair<int, int> cur_loc = {1, 0};

    std::pair<int, int> token_loc = {1, 0}; //마지막으로 읽은 토큰의 시작 위치

    static std::unordered_map<std::string_view, Token> token_map;

    int inner_advance();

    void advance();
};

#endif //ZULLANG_LEXER_H
