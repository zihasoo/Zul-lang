
//SPDX-FileCopyrightText: © 2023 ByungYun Lee
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULLANG_PARSER_H
#define ZULLANG_PARSER_H

#include <string_view>
#include <unordered_map>
#include <set>
#include <map>
#include <utility>
#include <memory>
#include <vector>
#include <sstream>
#include <tuple>

#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/TargetParser/Host.h"

#include "ZulContext.h"
#include "System.h"
#include "Utility.h"
#include "Lexer.h"
#include "AST.h"

class Parser {
public:
    explicit Parser(const std::string &source_name);

    std::pair<std::unique_ptr<llvm::LLVMContext>, std::unique_ptr<llvm::Module>> parse();

private:
    ZulContext zulctx;

    Lexer lexer;

    Token cur_tok;

    int cur_ret_type = -1;

    void parse_top_level();

    void parse_global_var();

    void parse_func_def(std::string &func_name, std::pair<int, int> name_loc, int target_level);

    std::tuple<std::vector<std::pair<std::string, int>>, bool, bool> parse_parameter();

    std::pair<std::vector<ASTPtr>, int> parse_block_body(int target_level);

    std::pair<ASTPtr, int> parse_line(int start_level, int target_level);

    ASTPtr parse_expr_start();

    ASTPtr parse_local_var(std::unique_ptr<LvalueAST> lvalue, Capture<std::string> name_cap);

    ASTPtr parse_expr();

    ASTPtr parse_bin_op(int prev_prec, ASTPtr left);

    ASTPtr parse_primary();

    std::pair<ASTPtr, bool> parse_if_header();

    std::pair<ASTPtr, int> parse_if(int target_level);

    std::pair<ASTPtr, int> parse_for(int target_level);

    ASTPtr parse_identifier();

    ASTPtr parse_func_call(std::string &name, std::pair<int, int> name_loc);

    std::unique_ptr<LvalueAST> parse_lvalue(std::string &name, std::pair<int, int> name_loc, bool check_exist = true);

    ASTPtr parse_subscript();

    std::pair<int, ASTPtr> parse_type(bool no_arr = false);

    ASTPtr parse_unary_op();

    ASTPtr parse_par();

    ASTPtr parse_str();

    ASTPtr parse_char();

    void create_func(FuncProtoAST &proto, const std::vector<ASTPtr> &body, std::pair<int, int> name_loc);

    void advance();

    int get_op_prec();

    std::map<std::string, FuncProtoAST> func_proto_map = {
            {STDIN_NAME, FuncProtoAST(STDIN_NAME, -1, {}, false, true)},
            {STDOUT_NAME, FuncProtoAST(STDOUT_NAME, -1, {}, false, true)},
            {"scanf", FuncProtoAST("scanf", id_int, {{"", id_char + TYPE_COUNTS}}, false, true)},
            {"printf", FuncProtoAST("printf", id_int, {{"", id_char + TYPE_COUNTS}}, false, true)},
    };

    std::map<std::string, int> type_map = {
            {"논리", 0},
            {"글자", 1},
            {"수",  2},
            {"실수", 3},
    };

    static std::unordered_map<Token, int> op_prec_map; //연산자 우선순위 맵
};


#endif //ZULLANG_PARSER_H
