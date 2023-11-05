#include "Lexer.h"
#include "Logger.h"
#include "System.h"

using std::string;
using std::cerr;
using std::ifstream;
using std::pair;
using std::set;
using std::initializer_list;
using std::string_view;

Lexer::Lexer(const string& source_name) : source(source_name) {
    last_word.reserve(30);
    cur_line.reserve(80);
    if (!source.is_open()) {
        cerr << "\"" << source_name << "\" 파일이 존재하지 않습니다.";
        exit(1);
    }
}

bool Lexer::advance() {
    last_char = source.get();
    if (last_char == '\n' || last_char == '\r' || last_char == EOF) {
        System::logger.register_line(cur_loc.first, cur_line);
        cur_line.clear();
        cur_loc.first++;
        cur_loc.second = 0;
        return true;
    } else {
        cur_line.push_back(last_char);
        cur_loc.second++;
        return false;
    }
}

void Lexer::release_error(const initializer_list<string_view> &msg) {
    //매모리의 반복 재할당을 막기 위해 initializer_list로 받고 한 번에 할당
    string str;
    int size = 0;
    for (const auto &x: msg) {
        size += x.size();
    }
    str.reserve(size);
    for (const auto &x: msg) {
        str.append(x);
    }
    System::logger.log_error(token_start_loc, last_word.size(), str);
    while (!advance()); //다음 줄로 이동
}

bool iskor(int c) {

}

bool iskornum(int c) {
    return iskor(c) || ('0' <= c && c <= '9');
}

Lexer::Token Lexer::get_token() {
    last_word.clear();
    while (isspace(last_char)) advance();

    token_start_loc = cur_loc;
    if (isalpha(last_char)) {
        while (isalnum(last_char)) {
            if (isalpha(last_char)) last_char = tolower(last_char);
            last_word.push_back(last_char);
            advance();
        }
//        if (last_word == "nop")
//            return tok_nop;
//        if (last_word == "add")
//            return tok_add;
//        if (last_word == "sub")
//            return tok_sub;
//        if (last_word == "cmp")
//            return tok_cmp;
//        if (last_word == "ja")
//            return tok_ja;
//        if (last_word == "jb")
//            return tok_jb;
//        if (last_word == "je")
//            return tok_je;
//        if (last_word == "jmp")
//            return tok_jmp;
//        if (last_word == "mov")
//            return tok_mov;

        return tok_identifier;
    }

    if ('0' <= last_char && last_char <= '9') {
        while ('0' <= last_char && last_char <= '9') {
            last_word.push_back(last_char);
            advance();
        }
        return tok_int;
    }

    if (last_char == EOF) {
        return tok_eof;
    }

    last_word.push_back(last_char);
    advance();
    if (last_word == "//") {
        while (!advance()); //주석 나오면 다음 줄까지 계속 이동
        while (isspace(last_char)) advance();
        return tok_anno;
    }
    if (last_word == ";") {
        return tok_semicolon;
    }
    if (last_word == ",") {
        return tok_comma;
    }
    return tok_undefined;
}

const string &Lexer::get_word() {
    return last_word;
}

pair<int, int> Lexer::get_cur_loc() {
    return cur_loc;
}

pair<int, int> Lexer::get_token_loc() {
    return token_start_loc;
}

string Lexer::token_to_string(Token token) {
    switch (token) {
        case tok_eof:
            return "tok_eof";
        case tok_identifier:
            return "tok_identifier";
        case tok_int:
            return "tok_int";
        case tok_undefined:
            return "tok_undefined";
        case tok_comma:
            return "tok_comma";
        case tok_semicolon:
            return "tok_semicolon";
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
        case tok_real:
            return "tok_real";
        case tok_string:
            return "tok_string";
        case tok_eq:
            return "tok_eq";
        case tok_gt:
            return "tok_gt";
        case tok_lt:
            return "tok_lt";
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
        case tok_asn:
            return "tok_asn";
        case tok_mul_asn:
            return "tok_mul_asn";
        case tok_div_asn:
            return "tok_div_asn";
        case tok_mod_asn:
            return "tok_mod_asn";
        case tok_add_asn:
            return "tok_add_asn";
        case tok_sub_asn:
            return "tok_sub_asn";
        case tok_lshift_asn:
            return "tok_lshift_asn";
        case tok_rshift_asn:
            return "tok_rshift_asn";
        case tok_and_asn:
            return "tok_and_asn";
        case tok_or_asn:
            return "tok_or_asn";
        case tok_xor_asn:
            return "tok_xor_asn";
        case tok_ineq:
            return "tok_ineq";
        case tok_gteq:
            return "tok_gteq";
        case tok_lteq:
            return "tok_lteq";
    }
}
