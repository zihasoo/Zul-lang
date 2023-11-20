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
using std::u16string_view;
using std::u16string;
using std::unordered_map;

Lexer::Lexer(string_view source_name) : source(source_name) {
    last_word.reserve(30);
    cur_line.reserve(80);
    if (!source.is_open()) {
        cerr << "에러: \"" << source_name << "\" 파일이 존재하지 않습니다.";
        exit(1);
    }
    advance();
}

void Lexer::release_error(const initializer_list<string_view> &msgs) {
    //매모리의 반복 재할당을 막기 위해 initializer_list로 받고 한 번에 할당
    string str;
    int size = 0;
    for (const auto &x: msgs) {
        size += x.size();
    }
    str.reserve(size);
    for (const auto &x: msgs) {
        str.append(x);
    }
    System::logger.log_error(token_start_loc, last_word.size(), std::move(str));
}


bool Lexer::advance() {
    last_char = source.get();
    cur_loc.second++;
    if (last_char == '\n' || last_char == '\r' || last_char == EOF) {
        System::logger.register_line(cur_loc.first, std::move(cur_line));
        cur_line.reserve(80);
        return true;
    } else {
        cur_line.push_back(last_char);
        int ret = 0;
        if ((last_char & 0x80) == 0) {
            ret = last_char;
        } else if ((last_char & 0xE0) == 0xC0) {
            ret = (last_char & 0x1F) << 6;
            last_char = source.get();
            cur_line.push_back(last_char);
            ret |= (last_char & 0x3F);
        } else if ((last_char & 0xF0) == 0xE0) {
            ret = (last_char & 0xF) << 12;
            last_char = source.get();
            cur_line.push_back(last_char);
            ret |= (last_char & 0x3F) << 6;
            last_char = source.get();
            cur_line.push_back(last_char);
            ret |= (last_char & 0x3F);
        } else if ((last_char & 0xF8) == 0xF0) {
            ret = (last_char & 0x7) << 18;
            last_char = source.get();
            cur_line.push_back(last_char);
            ret |= (last_char & 0x3F) << 12;
            last_char = source.get();
            cur_line.push_back(last_char);
            ret |= (last_char & 0x3F) << 6;
            last_char = source.get();
            cur_line.push_back(last_char);
            ret |= (last_char & 0x3F);
        }
        last_char = ret;
        return false;
    }
}

Lexer::Token Lexer::get_token() {
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
    }

    last_word.clear();
    while (isspace(last_char))
        advance();

    token_start_loc = cur_loc;
    if (iskor(last_char)) {
        while (iskornum(last_char) || last_char == '?') {
            last_word.push_back(last_char);
            advance();
        }
        if (token_map.contains(last_word))
            return token_map[last_word];

        return tok_identifier;
    }

    if (isalpha(last_char)) {
        while (isalnum(last_char)) {
            last_word.push_back(last_char);
            advance();
        }
        return tok_eng;
    }

    if (isnum(last_char) || last_char == '.') {
        bool isreal = false, wrong = false;
        while (isnum(last_char) || last_char == '.') {
            if (last_char == '.') {
                if (isreal)
                    wrong = true;
                isreal = true;
            }
            last_word.push_back(last_char);
            advance();
        }
        if (wrong) return tok_undefined;
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
        while (!advance());
        return get_token();
    }

    return token_map[last_word];
}

u16string Lexer::get_word() {
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
        case tok_inc:
            return "tok_inc";
        case tok_dec:
            return "tok_dec";
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
        case tok_undefined:
            return "tok_undefined";
    }
}

unordered_map<u16string_view, Lexer::Token> Lexer::token_map =
        {{u"ㅎㅇ",  tok_hi},
         {u"ㄱㄱ",  tok_go},
         {u"ㅇㅈ?", tok_ij},
         {u"ㄴㄴ?", tok_no},
         {u"ㄴㄴ",  tok_nope},
         {u"ㅈㅈ",  tok_gg},
         {u"ㅅㄱ",  tok_sg},
         {u"ㅌㅌ",  tok_tt},

         {u",",   tok_comma},
         {u":",   tok_colon},
         {u";",   tok_semicolon},
         {u"(",   tok_lpar},
         {u")",   tok_rpar},
         {u"[",   tok_lsqbrk},
         {u"]",   tok_rsqbrk},
         {u"{",   tok_lbrk},
         {u"}",   tok_rbrk},
         {u".",   tok_dot},
         {u"\"",  tok_dquotes},
         {u"'",   tok_squotes},
         {u"//",  tok_anno},

         {u"+",   tok_add},
         {u"-",   tok_sub},
         {u"*",   tok_mul},
         {u"/",   tok_div},
         {u"%",   tok_mod},
         {u"++",  tok_inc},
         {u"--",  tok_dec},

         {u"&&",  tok_and},
         {u"||",  tok_or},
         {u"!",   tok_not},
         {u"&",   tok_bitand},
         {u"|",   tok_bitor},
         {u"~",   tok_bitnot},
         {u"^",   tok_bitxor},
         {u"<<",  tok_lshift},
         {u">>",  tok_rshift},

         {u"=",   tok_assn},
         {u"*=",  tok_mul_assn},
         {u"/=",  tok_div_assn},
         {u"%=",  tok_mod_assn},
         {u"+=",  tok_add_assn},
         {u"-=",  tok_sub_assn},
         {u"<<=", tok_lshift_assn},
         {u">>=", tok_rshift_assn},
         {u"&=",  tok_and_assn},
         {u"|=",  tok_or_assn},
         {u"^=",  tok_xor_assn},

         {u"==",  tok_eq},
         {u"!=",  tok_ineq},
         {u">",   tok_gt},
         {u">=",  tok_gteq},
         {u"<",   tok_lt},
         {u"<=",  tok_lteq}};
