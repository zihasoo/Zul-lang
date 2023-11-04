#ifndef ZULLANG_LEXER_H
#define ZULLANG_LEXER_H

#include <fstream>
#include <string>
#include <utility>
#include <set>
#include <map>
#include <initializer_list>

class Lexer {
public:
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

        // primary
        tok_identifier,
        tok_int,
        tok_real,
        tok_string,

        // operators
        tok_comma, // ,
        tok_semicolon, // ;
        tok_left_par, // (
        tok_right_par, // )
        tok_eq, // =
        tok_gt,// >
        tok_lt, // <

        //eof
        tok_eof,

        //undefined
        tok_undefined
    };

    explicit Lexer(const std::string& source_name);

    Token get_token(); //현재 입력 스트림에서 토큰 타입(enum) 얻기

    const std::string& get_word(); //last_word getter

    std::pair<int, int> get_cur_loc(); //cur_loc getter

    std::pair<int, int> get_token_loc(); //last_token_loc getter

    void release_error(const std::initializer_list<std::string_view>& msg);

    static std::string token_to_string(Token token);

private:
    std::string cur_line; //현재 줄

    int last_char = ' '; //마지막으로 읽은 글자

    std::string last_word; //get_token 함수로부터 얻어진 단어 (string)

    std::ifstream source; //입력 파일

    std::pair<int, int> cur_loc = {1, 0}; //row, col. 현재 lexer가 가리키고 있는 위치

    std::pair<int, int> last_token_loc = {1, 0}; //마지막으로 읽은 토큰의 시작 위치

    bool advance(); //현재 입력 스트림에서 한 글자 얻고 위치 기록 (줄바꿈 되면 true 리턴)
};

#endif //ZULLANG_LEXER_H
