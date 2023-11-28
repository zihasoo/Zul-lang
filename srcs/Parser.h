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
#include "llvm/TargetParser/Host.h"

#include "System.h"
#include "Utility.h"
#include "Lexer.h"
#include "AST.h"

class Parser {
public:
    explicit Parser(const std::string &source_name);

    void parse();

    ZulContext& get_zulctx();

private:
    ZulContext zulctx;

    Lexer lexer;

    Token cur_tok;

    void parse_top_level();

    void parse_global_var();

    void parse_func_def(std::string &func_name, std::pair<int,int> name_loc);

    std::vector<std::pair<std::string, int>> parse_parameter();

    std::unique_ptr<AST> parse_expr_start(std::string& name);

    std::unique_ptr<AST> parse_local_var(std::string &name, std::pair<int, int> name_loc);

    std::unique_ptr<AST> parse_expr();

    std::unique_ptr<AST> parse_bin_op(int prev_prec, std::unique_ptr<AST> left);

    std::unique_ptr<AST> parse_primary();

    std::unique_ptr<AST> parse_identifier();

    std::unique_ptr<AST> parse_identifier(std::string &name, std::pair<int, int> name_loc);

    std::unique_ptr<UnaryOpAST> parse_unary_op();

    std::unique_ptr<AST> parse_par();

    std::unique_ptr<ImmStrAST> parse_str();

    void advance();

    int get_op_prec();

    std::map<std::string, FuncProtoAST> func_proto_map;

    std::map<std::string, int> type_map = {
            {"논리", 0},
            {"글자", 1},
            {"수",  2},
            {"소수", 3},
            {"글",  4}
    };

    static std::unordered_map<Token, int> op_prec_map; //연산자 우선순위 맵
};


#endif //ZULLANG_PARSER_H
