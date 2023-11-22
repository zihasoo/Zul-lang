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

using namespace std;
using namespace llvm;

class Parser {
public:
    explicit Parser(const string &source_name);

    void parse();

private:
    unique_ptr<LLVMContext> context;
    unique_ptr<Module> module;
    unique_ptr<IRBuilder<>> builder;

    Lexer lexer;

    Token cur_tok;

    void parse_top_level();

    void parse_global_var();

    unique_ptr<FuncAST> parse_func_def(string &func_name);

    vector<VariableAST> parse_parameter();

    unique_ptr<AST> parse_expr();

    unique_ptr<AST> parse_bin_op(int prev_prec, unique_ptr<AST> left);

    unique_ptr<AST> parse_primary();

    unique_ptr<AST> parse_identifier();

    unique_ptr<AST> parse_par();

    void advance();

    int get_op_prec();

    map<string, int> type_map = {
            {"수",  0},
            {"소수", 1},
            {"글",  2}
    };

    static unordered_map<Token, int> op_prec_map; //연산자 우선순위 맵
};


#endif //ZULLANG_PARSER_H
