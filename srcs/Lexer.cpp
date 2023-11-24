#include "Lexer.h"
#include "Logger.h"
#include "System.h"
#include "Utility.h"

using std::string;
using std::cerr;
using std::ifstream;
using std::pair;
using std::initializer_list;
using std::string_view;
using std::unordered_map;

Lexer::Lexer(const string &source_name) : source(source_name) {
    last_word.reserve(30);
    cur_line.reserve(80);
    if (!source.is_open()) {
        cerr << "에러: \"" << source_name << "\" 파일이 존재하지 않습니다.";
        exit(1);
    }
    advance();
}

void Lexer::log_cur_token(string msg) {
    System::logger.log_error(token_start_loc, last_word.size(), std::move(msg));
}

void Lexer::log_cur_token(const std::initializer_list<std::string_view> &msgs) {
    System::logger.log_error(token_start_loc, last_word.size(), msgs);
}

void Lexer::advance() {
    last_char = source.get();
    cur_loc.second++;
    raw_last_char.clear();
    raw_last_char.push_back(last_char);
    if (last_char == '\n' || last_char == EOF) {
        System::logger.register_line(cur_loc.first, std::move(cur_line));
        cur_line.reserve(80);
    } else {
        cur_line.push_back(last_char);
        int ret = 0;
        if ((last_char & 0x80) == 0) {
            ret = last_char;
        } else if ((last_char & 0xE0) == 0xC0) {
            ret = (last_char & 0x1F) << 6;
            last_char = source.get();
            cur_line.push_back(last_char);
            raw_last_char.push_back(last_char);
            ret |= (last_char & 0x3F);
        } else if ((last_char & 0xF0) == 0xE0) {
            ret = (last_char & 0xF) << 12;
            last_char = source.get();
            cur_line.push_back(last_char);
            raw_last_char.push_back(last_char);
            ret |= (last_char & 0x3F) << 6;
            last_char = source.get();
            cur_line.push_back(last_char);
            raw_last_char.push_back(last_char);
            ret |= (last_char & 0x3F);
        } else if ((last_char & 0xF8) == 0xF0) {
            ret = (last_char & 0x7) << 18;
            last_char = source.get();
            cur_line.push_back(last_char);
            raw_last_char.push_back(last_char);
            ret |= (last_char & 0x3F) << 12;
            last_char = source.get();
            cur_line.push_back(last_char);
            raw_last_char.push_back(last_char);
            ret |= (last_char & 0x3F) << 6;
            last_char = source.get();
            cur_line.push_back(last_char);
            raw_last_char.push_back(last_char);
            ret |= (last_char & 0x3F);
        }
        last_char = ret;
    }
}

Token Lexer::get_token() {
    static bool is_line_start = false;

    last_word.clear();
    token_start_loc = cur_loc;
    if (last_char == '\n') {
        is_line_start = true;
        cur_loc.first++;
        cur_loc.second = 0;
        last_word.push_back(last_char);
        advance();
        return tok_newline;
    }
    if (is_line_start) {
        while (last_char == ' ') {
            last_word.push_back(last_char);
            advance();
            if (last_word.size() == 4) {
                return tok_indent;
            }
        }
        is_line_start = false;
        if (0 < last_word.size() && last_word.size() < 4) {
            log_cur_token("들여쓰기가 잘못되었습니다");
        }
    }

    last_word.clear();
    while (last_char != '\n' && isspace(last_char))
        advance();

    token_start_loc = cur_loc;
    if (iskor(last_char) || last_char == '_') {
        while (iskornum(last_char) || last_char == '_' || last_char == '?') {
            last_word.append(raw_last_char);
            advance();
        }
        if (token_map.contains(last_word))
            return token_map[last_word];

        return tok_identifier;
    }

    if (isalpha(last_char) || last_char == '_') {
        while (isalnum(last_char) || last_char == '_') {
            last_word.append(raw_last_char);
            advance();
        }
        return tok_identifier;
    }

    if (isnum(last_char) || last_char == '.') {
        bool isreal = false, wrong = false;
        while (isnum(last_char) || last_char == '.') {
            if (last_char == '.') {
                if (isreal)
                    wrong = true;
                isreal = true;
            }
            last_word.append(raw_last_char);
            advance();
        }
        if (wrong) {
            log_cur_token("잘못된 실수 표현입니다");
            return tok_undefined;
        }
        return isreal ? tok_real : tok_int;
    }

    if (last_char == EOF) {
        return tok_eof;
    }

    while (true) {
        last_word.push_back(last_char);
        if (!token_map.contains(last_word))
            break;
        advance();
    }

    if (last_word.size() <= 1) {
        advance();
        return tok_undefined;
    }

    last_word.pop_back();

    if (token_map[last_word] == tok_anno) {
        while (last_char != '\n')
            advance();
        return get_token();
    }

    return token_map[last_word];
}

string& Lexer::get_word() {
    return last_word;
}

pair<int, int> Lexer::get_cur_loc() {
    return cur_loc;
}

pair<int, int> Lexer::get_token_start_loc() {
    return token_start_loc;
}

string Lexer::token_to_string(Token token) {
    switch (token) {
        case tok_hi:
            return "tok_hi";
        case tok_go:
            return "tok_go";
        case tok_ij:
            return "tok_ij";
        case tok_no:
            return "tok_no";
        case tok_nope:
            return "tok_nope";
        case tok_gg:
            return "tok_gg";
        case tok_sg:
            return "tok_sg";
        case tok_tt:
            return "tok_tt";
        case tok_identifier:
            return "tok_identifier";
        case tok_int:
            return "tok_int";
        case tok_real:
            return "tok_real";
        case tok_string:
            return "tok_string";
        case tok_eng:
            return "tok_eng";
        case tok_indent:
            return "tok_indent";
        case tok_newline:
            return "tok_newline";
        case tok_comma:
            return "tok_comma";
        case tok_colon:
            return "tok_colon";
        case tok_semicolon:
            return "tok_semicolon";
        case tok_lpar:
            return "tok_lpar";
        case tok_rpar:
            return "tok_rpar";
        case tok_rsqbrk:
            return "tok_rsqbrk";
        case tok_lsqbrk:
            return "tok_lsqbrk";
        case tok_rbrk:
            return "tok_rbrk";
        case tok_lbrk:
            return "tok_lbrk";
        case tok_dot:
            return "tok_dot";
        case tok_dquotes:
            return "tok_dquotes";
        case tok_squotes:
            return "tok_squotes";
        case tok_anno:
            return "tok_anno";
        case tok_add:
            return "tok_add";
        case tok_sub:
            return "tok_sub";
        case tok_mul:
            return "tok_mul";
        case tok_div:
            return "tok_div";
        case tok_mod:
            return "tok_mod";
        case tok_and:
            return "tok_and";
        case tok_or:
            return "tok_or";
        case tok_not:
            return "tok_not";
        case tok_bitand:
            return "tok_bitand";
        case tok_bitor:
            return "tok_bitor";
        case tok_bitnot:
            return "tok_bitnot";
        case tok_bitxor:
            return "tok_bitxor";
        case tok_lshift:
            return "tok_lshift";
        case tok_rshift:
            return "tok_rshift";
        case tok_assn:
            return "tok_assn";
        case tok_mul_assn:
            return "tok_mul_assn";
        case tok_div_assn:
            return "tok_div_assn";
        case tok_mod_assn:
            return "tok_mod_assn";
        case tok_add_assn:
            return "tok_add_assn";
        case tok_sub_assn:
            return "tok_sub_assn";
        case tok_lshift_assn:
            return "tok_lshift_assn";
        case tok_rshift_assn:
            return "tok_rshift_assn";
        case tok_and_assn:
            return "tok_and_assn";
        case tok_or_assn:
            return "tok_or_assn";
        case tok_xor_assn:
            return "tok_xor_assn";
        case tok_eq:
            return "tok_eq";
        case tok_ineq:
            return "tok_ineq";
        case tok_gt:
            return "tok_gt";
        case tok_gteq:
            return "tok_gteq";
        case tok_lt:
            return "tok_lt";
        case tok_lteq:
            return "tok_lteq";
        case tok_eof:
            return "tok_eof";
        default:
            return "tok_undefined";
    }
}

unordered_map<string_view, Token> Lexer::token_map =
        {{"ㅎㅇ",  tok_hi},
         {"ㄱㄱ",  tok_go},
         {"ㅇㅈ?", tok_ij},
         {"ㄴㄴ?", tok_no},
         {"ㄴㄴ",  tok_nope},
         {"ㅈㅈ",  tok_gg},
         {"ㅅㄱ",  tok_sg},
         {"ㅌㅌ",  tok_tt},

         {",",   tok_comma},
         {":",   tok_colon},
         {";",   tok_semicolon},
         {"(",   tok_lpar},
         {")",   tok_rpar},
         {"[",   tok_lsqbrk},
         {"]",   tok_rsqbrk},
         {"{",   tok_lbrk},
         {"}",   tok_rbrk},
         {".",   tok_dot},
         {"\"",  tok_dquotes},
         {"'",   tok_squotes},
         {"//",  tok_anno},

         {"+",   tok_add},
         {"-",   tok_sub},
         {"*",   tok_mul},
         {"/",   tok_div},
         {"%",   tok_mod},

         {"&&",  tok_and},
         {"||",  tok_or},
         {"!",   tok_not},
         {"&",   tok_bitand},
         {"|",   tok_bitor},
         {"~",   tok_bitnot},
         {"^",   tok_bitxor},
         {"<<",  tok_lshift},
         {">>",  tok_rshift},

         {"=",   tok_assn},
         {"*=",  tok_mul_assn},
         {"/=",  tok_div_assn},
         {"%=",  tok_mod_assn},
         {"+=",  tok_add_assn},
         {"-=",  tok_sub_assn},
         {"<<=", tok_lshift_assn},
         {">>=", tok_rshift_assn},
         {"&=",  tok_and_assn},
         {"|=",  tok_or_assn},
         {"^=",  tok_xor_assn},

         {"==",  tok_eq},
         {"!=",  tok_ineq},
         {">",   tok_gt},
         {">=",  tok_gteq},
         {"<",   tok_lt},
         {"<=",  tok_lteq}};
