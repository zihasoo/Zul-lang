#ifndef ZULLANG_PARSER_H
#define ZULLANG_PARSER_H

#include <string_view>
#include <unordered_map>
#include <utility>
#include <memory>
#include <vector>
#include "System.h"
#include "Lexer.h"
#include "AST.h"

using namespace std;

class Parser {
public:
    explicit Parser(const string &source_name) : lexer(source_name) {
        advance();
    }

    void parse() {
        parse_top_level();
    }

    unordered_map<u16string, int>& get_global_var_map() {
        return global_var_map;
    }

    unordered_map<u16string, FuncAST>& get_func_map() {
        return func_map;
    }

private:
    Lexer lexer;

    Token cur_tok;

    void advance() { cur_tok = lexer.get_token(); }

    void parse_top_level() {
        while (true) {
            if (cur_tok == tok_eof)
                break;
            if (cur_tok == tok_newline) {
                advance();
                continue;
            }
            if (cur_tok == tok_identifier) { //전역 변수 선언
                parse_global_var();
            } else if (cur_tok == tok_hi) {
                advance();
                if (cur_tok != tok_identifier) {
                    lexer.log_cur_token("이름이 와야 합니다");
                    advance();
                    continue;
                }
                auto name = lexer.get_word();
                advance();
                if (cur_tok == tok_colon) { //클래스 정의
                    cerr << "클래스 정의입니다.\n";
                    advance();
                } else if (cur_tok == tok_lpar) { //함수 정의
                    advance();
                    parse_function(name);
                } else {
                    lexer.log_cur_token("잘못된 토큰입니다");
                    advance();
                }
            } else {
                lexer.log_cur_token("잘못된 토큰입니다");
                advance();
            }
        }
    }

    void parse_global_var() {
        auto var_name = lexer.get_word();
        advance();
        if (cur_tok == tok_colon) { //선언만
            advance();
            if (cur_tok != tok_identifier) {
                lexer.log_cur_token("타입이 와야 합니다");
                advance();
            }
            auto type_name = lexer.get_word();
            if (type_map.contains(type_name)) {
                if (global_var_map.contains(var_name))
                    lexer.log_cur_token("변수가 재 정의 되었습니다.");
                global_var_map[var_name] = type_map[type_name];
                advance();
            } else {
                lexer.log_cur_token("존재하지 않는 타입입니다");
                advance();
            }

        } else if (cur_tok == tok_assn) { //선언과 초기화
            lexer.log_cur_token("아직 전역변수 초기화가 지원되지 않습니다");
            while (cur_tok != tok_newline)
                advance();
        } else {
            lexer.log_cur_token("예기치 않은 토큰");
            advance();
        }
    }

    void parse_function(u16string& func_name) {
        auto params = parse_parameter();
        int return_type;
        if (cur_tok == tok_identifier) {
            auto type_name = lexer.get_word();
            if (type_map.contains(type_name)) {
                return_type = type_map[type_name];
            }
            else {
                lexer.log_cur_token("존재하지 않는 타입입니다.");
            }
            advance();
        }
        if (cur_tok != tok_colon)
            lexer.log_cur_token("콜론이 와야 합니다");
        advance();
        if (cur_tok != tok_newline)
            lexer.log_cur_token("예기치 않은 토큰");
        advance();
        if (cur_tok != tok_indent)
            lexer.log_cur_token("들여쓰기가 필요합니다");
        advance();

        //TODO 함수 몸체 파싱하는 함수 필요 -> 함수 반환값 추론도 해야함
        Token before = cur_tok;
        auto ast = make_unique<AST>();
        while (true) {
            advance();
            if (cur_tok == tok_eof)
                break;
            if (before == tok_newline && cur_tok != tok_newline && cur_tok != tok_indent)
                break;
            before = cur_tok;
        }
        func_map.emplace(std::move(func_name), FuncAST(return_type, std::move(params), std::move(ast)));
    }

    vector<VariableAST> parse_parameter() {
        vector<VariableAST> args;
        if (cur_tok == tok_rpar) {
            advance();
            return args;
        }
        while (true) {
            if (cur_tok != tok_identifier)
                lexer.log_cur_token("함수의 매개변수가 와야 합니다");
            auto name = lexer.get_word();
            advance();
            if (cur_tok != tok_colon)
                lexer.log_cur_token("예기치 않은 토큰. 콜론이 와야 합니다");
            advance();
            if (cur_tok != tok_identifier)
                lexer.log_cur_token("매개변수에는 타입을 명시해야 합니다.");
            auto type = lexer.get_word();
            if (type_map.contains(type))
                args.emplace_back(name, type_map[type]);
            else
                lexer.log_cur_token("존재하지 않는 타입입니다.");
            advance();
            if (cur_tok == tok_rpar)
                break;
            if (cur_tok != tok_comma) {
                lexer.log_cur_token("콤마가 와야 합니다");
            }
            advance();
        }
        advance();
        return args;
    }

    void parse_expr() {

    }

    unique_ptr<BinOpAST> parse_bin_op(Token op, unique_ptr<AST> left) {
        return nullptr;
    }

    int get_op_prec() {
        if (op_prec_map.contains(cur_tok)) {
            return op_prec_map[cur_tok];
        }
        return -1;
    }

    unordered_map<u16string, int> global_var_map;

    unordered_map<u16string, int> type_map = {
            {u"수", 0},
            {u"소수", 1},
            {u"글", 2}
    };

    unordered_map<u16string, FuncAST> func_map;

    static unordered_map<Token, int> op_prec_map; //연산자 우선순위 맵
};


#endif //ZULLANG_PARSER_H
