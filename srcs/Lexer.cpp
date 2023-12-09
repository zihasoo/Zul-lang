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

void Lexer::log_unexpected(string_view msg) {
    if (last_char == EOF)
        log_token({"예기치 않은 EOF. ", msg});
    else if (last_word.empty())
        log_token({"예기치 않은 줄바꿈. ", msg});
    else
        log_token({"예기치 않은 토큰 \"", last_word, "\" ", msg});
}

void Lexer::log_token(string_view msg) {
    System::logger.log_error(token_loc, last_word.size(), msg);
}

void Lexer::log_token(const std::initializer_list<std::string_view> &msgs) {
    System::logger.log_error(token_loc, last_word.size(), msgs);
}

int Lexer::inner_advance() {
    int input = source.get();
    cur_line.push_back(input);
    raw_last_char.push_back(input);
    return input;
}

void Lexer::advance() {
    int input = source.get();
    cur_loc.second++;
    raw_last_char.clear();
    if (input != '\n' && input != EOF) {
        cur_line.push_back(input);
        raw_last_char.push_back(input);
    }
    if ((input & 0xE0) == 0xC0) {
        last_char = (input & 0x1F) << 6;
        last_char |= (inner_advance() & 0x3F);
    } else if ((input & 0xF0) == 0xE0) {
        last_char = (input & 0xF) << 12;
        last_char |= (inner_advance() & 0x3F) << 6;
        last_char |= (inner_advance() & 0x3F);
    } else if ((input & 0xF8) == 0xF0) {
        last_char = (input & 0x7) << 18;
        last_char |= (inner_advance() & 0x3F) << 12;
        last_char |= (inner_advance() & 0x3F) << 6;
        last_char |= (inner_advance() & 0x3F);
    } else {
        last_char = input;
    }
}

Token Lexer::get_token() {
    static bool is_line_start = false;

    last_word.clear();

    if (is_line_start) {
        token_loc = cur_loc;
        while (last_char == ' ') {
            last_word.push_back(last_char);
            advance();
            if (last_word.size() == 4) {
                return tok_indent;
            }
        }
        if (!last_word.empty() && last_word.size() < 4) {
            log_token("잘못된 들여쓰기입니다");
            return tok_indent;
        }
        if (last_char == '\t') {
            advance();
            log_token("반드시 공백 문자 4개로 들여쓰기를 해야 합니다. 탭 문자는 허용되지 않습니다");
        }
        is_line_start = false;
        last_word.clear();
    }
    while (last_char != '\n' &&
           #ifdef DEBUG
           -1 <= last_char && last_char <= 255 &&
           #endif
           isspace(last_char))
        advance();

    token_loc = cur_loc;

    if (last_char == '\n') {
        is_line_start = true;
        System::logger.register_line(cur_loc.first, std::move(cur_line));
        cur_line.reserve(80);
        cur_loc.first++;
        cur_loc.second = 0;
        advance();
        return tok_newline;
    }

    if (iskor(last_char) || last_char == '_') {
        while (iskornum(last_char) || last_char == '_' || last_char == '?') {
            last_word.append(raw_last_char);
            advance();
        }
        if (token_map.contains(last_word))
            return token_map[last_word];

        return tok_identifier;
    }

    if (
        #ifdef DEBUG
        -1 <= last_char && last_char <= 255 &&
        #endif
        isalpha(last_char) || last_char == '_') {
        while (isalnum(last_char) || last_char == '_') {
            last_word.append(raw_last_char);
            advance();
        }
        return tok_identifier;
    }

    if (isnum(last_char) || last_char == '.') {
        bool isreal = false, wrong = false, digit = false;
        while (isnum(last_char) || last_char == '.') {
            if (last_char == '.') {
                if (isreal)
                    wrong = true;
                isreal = true;
            } else {
                digit = true;
            }
            last_word.append(raw_last_char);
            advance();
        }
        if (last_word == "...")
            return tok_va_arg;
        if (wrong) {
            if (digit)
                log_token("잘못된 실수 표현입니다");
            return tok_undefined;
        }
        return isreal ? tok_real : tok_int;
    }

    if (last_char == EOF) {
        System::logger.register_line(cur_loc.first, std::move(cur_line));
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
    } else if (last_word == "++" || last_word == "--") {
        log_token("줄랭에는 '++', '--' 단항 연산자가 존재하지 않습니다");
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

string &Lexer::get_word() {
    return last_word;
}

pair<int, int> Lexer::get_token_loc() {
    return token_loc;
}

int Lexer::get_line_index() {
    return cur_line.size() - raw_last_char.size() - 1;
}

std::string Lexer::get_line_substr(int st, int ed) {
    return cur_line.substr(st, ed - st);
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
         {"참",   tok_true},
         {"거짓",  tok_false},

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
         {"...", tok_va_arg},

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
