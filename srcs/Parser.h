#ifndef ZULLANG_PARSER_H
#define ZULLANG_PARSER_H

#include <string_view>
#include <unordered_map>
#include <map>
#include <utility>
#include <memory>
#include <vector>

#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"

#include "System.h"
#include "Utility.h"
#include "Lexer.h"
#include "AST.h"

class Parser {
public:
    explicit Parser(const std::string &source_name);

    void parse();

private:
    IRTools ir_tools;

    Lexer lexer;

    Token cur_tok;

    void parse_top_level();

    void parse_global_var();

    std::unique_ptr<FuncAST> parse_func_def(std::string &func_name);

    std::vector<VariableAST> parse_parameter();

    std::unique_ptr<AST> parse_expr();

    std::unique_ptr<AST> parse_bin_op(int prev_prec, std::unique_ptr<AST> left);

    std::unique_ptr<AST> parse_primary();

    std::unique_ptr<AST> parse_identifier();

    std::unique_ptr<AST> parse_par();

    void advance();

    int get_op_prec();

    std::map<std::string, std::unique_ptr<FuncProtoAST>> func_proto_map;

    std::map<std::string, int> type_map = {
            {"수",  0},
            {"소수", 1},
            {"글",  2}
    };

    static std::unordered_map<Token, int> op_prec_map; //연산자 우선순위 맵
};


#endif //ZULLANG_PARSER_H
