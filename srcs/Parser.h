#ifndef ZULLANG_PARSER_H
#define ZULLANG_PARSER_H

#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "System.h"
#include "Lexer.h"
#include "AST.h"

class Parser {
public:
    explicit Parser(std::string_view source_name) : lexer(source_name) {}

    void parse() {
        parse_top_level();
    }

private:
    Lexer lexer;

    Lexer::Token cur_tok = Lexer::tok_undefined;

    void advance() { cur_tok = lexer.get_token(); }

    void parse_top_level() {
        std::u16string name;
        while (true) {
            advance();
            if (cur_tok == Lexer::tok_eof)
                break;
            if (cur_tok == Lexer::tok_identifier) { //전역 변수 선언
                name = lexer.get_word();
                advance();
                if (cur_tok == Lexer::tok_colon) { //선언만

                }
                else if (cur_tok == Lexer::tok_assn) { //선언과 초기화

                }
            }
            else if (cur_tok == Lexer::tok_hi) {
                advance();
                if (cur_tok != Lexer::tok_identifier) {
                    lexer.release_error({"이름이 와야 합니다."});
                    continue;
                }
                name = lexer.get_word();
                advance();
                if (cur_tok == Lexer::tok_colon) { //클래스 정의
                    std::cerr << "클래스 정의입니다.\n";
                }
                else if (cur_tok == Lexer::tok_lpar) { //함수 정의
                    parse_args();
                }
            }
            else {
                lexer.release_error({"잘못된 토큰입니다."});
            }
        }
    }

    void parse_expr() {

    }

    void parse_args() {
        std::vector<std::unique_ptr<AST>> args;
        while (true) {
            advance();
            if (cur_tok == Lexer::tok_rpar)
                break;

        }
    }

    void parse_function() {

    }

    static std::unordered_map<Lexer::Token, int> op_prec_map; //연산자 우선순위 맵
};


#endif //ZULLANG_PARSER_H
