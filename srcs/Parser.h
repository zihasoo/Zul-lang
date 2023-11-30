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

    int cur_func_ret_type = -1;

    void parse_top_level();

    void parse_global_var();

    void parse_func_def(std::string &func_name, std::pair<int,int> name_loc, int target_level);

    std::pair<std::vector<std::pair<std::string, int>>, bool> parse_parameter();

    std::pair<ASTPtr, int> parse_line(int start_level, int target_level);

    ASTPtr parse_expr_start();

    ASTPtr parse_local_var(std::string &name, std::pair<int, int> name_loc);

    ASTPtr parse_expr();

    ASTPtr parse_bin_op(int prev_prec, ASTPtr left);

    ASTPtr parse_primary();

    std::pair<ASTPtr, int> parse_if(int target_level);

    std::pair<ASTPtr, int> parse_for(int target_level);

    ASTPtr parse_identifier();

    ASTPtr parse_identifier(std::string &name, std::pair<int, int> name_loc);

    ASTPtr parse_unary_op();

    ASTPtr parse_par();

    ASTPtr parse_str();

    ASTPtr parse_char();

    void create_func(FuncProtoAST &proto, const std::vector<ASTPtr>& body);

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
