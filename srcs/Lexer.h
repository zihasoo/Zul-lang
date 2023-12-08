#ifndef ZULLANG_LEXER_H
#define ZULLANG_LEXER_H

#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <initializer_list>

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
    tok_eng,
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

    Token get_token(); //현재 입력 스트림에서 토큰 타입(enum) 얻기

    std::string& get_word(); //last_word getter

    std::pair<int, int> get_cur_loc(); //cur_loc getter

    std::pair<int, int> get_token_start_loc(); //token_start_loc getter

    int get_line_index();

    std::string get_line_substr(int st, int ed);

    void log_cur_token(std::string msg);  //현재 토큰을 기반으로 에러를 로깅함

    void log_cur_token(const std::initializer_list<std::string_view>& msgs); //분할 된 메세지를 받는 오버로드

private:
    std::string cur_line; //현재 줄

    int last_char = ' '; //마지막으로 읽은 글자 (유니코드)

    std::string last_word; //get_token 함수로부터 얻어진 단어 (string)

    std::string raw_last_char; //utf8 한 글자를 그대로 저장하기 위한 버퍼

    std::ifstream source; //입력 파일

    std::pair<int, int> cur_loc = {1, 0}; //row, col. 현재 lexer가 가리키고 있는 위치

    std::pair<int, int> token_start_loc = {1, 0}; //마지막으로 읽은 토큰의 시작 위치

    static std::unordered_map<std::string_view, Token> token_map; //토큰 문자열 - Token값 해시맵

    int inner_advance(); //입력 스트림에서 1바이트를 읽고 저장한 뒤 반환

    void advance(); //입력 스트림에서 바이트를 적절히 읽어들여 유니코드로 변환 후 last_char에 저장
};

#endif //ZULLANG_LEXER_H
