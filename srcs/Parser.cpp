#include "Parser.h"

using namespace std;
using namespace llvm;

Parser::Parser(const string &source_name) : lexer(source_name) {
    advance();
    context = make_unique<LLVMContext>();
    module = make_unique<Module>("zul", *context);
    builder = make_unique<IRBuilder<>>(*context);
}

void Parser::parse() {
    parse_top_level();
    module->print(errs(), nullptr);
}

void Parser::advance() {
    cur_tok = lexer.get_token();
}

int Parser::get_op_prec() {
    if (op_prec_map.contains(cur_tok))
        return op_prec_map[cur_tok];
    return -1;
}

void Parser::parse_top_level() {
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
                auto fn_ast = parse_func_def(name);
                auto fn_ir= fn_ast->code_gen(context.get(), module.get(), builder.get());
            } else {
                lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
                advance();
            }
        } else {
            lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
            advance();
        }
    }
}

void Parser::parse_global_var() {
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
            int type_num = type_map[type_name];
            auto type = get_llvm_type(context.get(), type_num);
            auto init_val = type_num == 0 ? llvm::ConstantInt::get(type, 0) : llvm::ConstantFP::get(type, 0);
            auto global_var = new llvm::GlobalVariable(*module, type, false, GlobalVariable::ExternalLinkage, init_val, var_name);
            if (global_var_map.contains(var_name))
                lexer.log_cur_token("변수가 다시 정의되었습니다.");
            global_var_map[var_name] = type_num;
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

unique_ptr<FuncAST> Parser::parse_func_def(string &func_name) {
    auto params = parse_parameter();
    int return_type = 0;
    if (cur_tok == tok_identifier) {
        auto type_name = lexer.get_word();
        if (type_map.contains(type_name)) {
            return_type = type_map[type_name];
        } else {
            lexer.log_cur_token({"\"", type_name, "\"은 존재하지 않는 타입입니다."});
        }
        advance();
    }
    if (cur_tok != tok_colon)
        lexer.log_cur_token("콜론이 와야 합니다");
    advance();
    if (cur_tok != tok_newline)
        lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
    advance();
    if (cur_tok != tok_indent)
        lexer.log_cur_token("들여쓰기가 필요합니다");
    advance();

    //TODO 함수 몸체 파싱하는 함수 필요 -> 함수 반환값 추론도 해야함
    Token before = cur_tok;
    while (true) {
        advance();
        if (cur_tok == tok_eof)
            break;
        if (before == tok_newline && cur_tok != tok_newline && cur_tok != tok_indent)
            break;
        //parse_primary();
        before = cur_tok;
    }

    auto func_proto = make_unique<FuncProtoAST>(std::move(func_name), return_type, std::move(params));

    return make_unique<FuncAST>(std::move(func_proto), std::make_unique<ImmIntAST>(0));
}

vector<VariableAST> Parser::parse_parameter() {
    vector<VariableAST> params;
    if (cur_tok == tok_rpar) {
        advance();
        return params;
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
            params.emplace_back(name, type_map[type]);
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
    return params;
}

unique_ptr<AST> Parser::parse_expr() {
    unique_ptr<AST> left = parse_primary();
    if (!left) {
        lexer.log_cur_token("예기치 않은 토큰");
        return nullptr;
    }
    return parse_bin_op(0, std::move(left));
}

unique_ptr<AST> Parser::parse_bin_op(int prev_prec, unique_ptr<AST> left) {
    while (true) {
        advance();
        if (!op_prec_map.contains(cur_tok)) {
            return left;
        }
    }
    return nullptr;
}

unique_ptr<AST> Parser::parse_primary() {
    switch (cur_tok) {
        case tok_identifier:
            return parse_identifier();
        case tok_real:
            return make_unique<ImmRealAST>(stod(lexer.get_word()));
        case tok_int:
            return make_unique<ImmIntAST>(stoll(lexer.get_word()));
        case tok_rpar:
            return parse_par();
        default:
            return nullptr;
    }
}


unique_ptr<AST> Parser::parse_par() {
    advance(); // (
    auto ret = parse_expr();
    if (cur_tok != tok_rpar) {
        lexer.log_cur_token(")가 필요합니다");
        advance();
        return nullptr;
    }
    advance(); // )
    return ret;
}

unique_ptr<AST> Parser::parse_identifier() {
    auto name = lexer.get_word();
    advance();
    if (cur_tok != tok_lpar)
        return make_unique<VariableAST>(name, 0);
    advance();
    vector<unique_ptr<AST>> args;
    while (true) {
        auto arg = parse_expr();
        if (!arg) return nullptr;
        args.push_back(std::move(arg));
        if (cur_tok == tok_rpar) {
            advance();
            return make_unique<FuncCallAST>(std::move(name), std::move(args));
        }
        if (cur_tok != tok_comma) {
            lexer.log_cur_token("콤마가 필요합니다");
            advance();
            return nullptr;
        }
        advance();
    }
}

std::unordered_map<Token, int> Parser::op_prec_map = {
        {tok_bitnot,      490}, // ~
        {tok_not,         490}, // !

        {tok_mul,         480}, // *
        {tok_div,         480}, // /
        {tok_mod,         480}, // %

        {tok_add,         470}, // +
        {tok_sub,         470}, // -

        {tok_lshift,      460}, // <<
        {tok_rshift,      460}, // >>

        {tok_lt,          450}, // <
        {tok_gt,          450}, // >
        {tok_lteq,        450}, // <=
        {tok_gteq,        450}, // >=

        {tok_eq,          440}, // ==
        {tok_ineq,        440}, // !=

        {tok_bitand,      430}, // &

        {tok_bitxor,      420}, // ^

        {tok_bitor,       410}, // |

        {tok_and,         400}, // &&

        {tok_or,          390}, // ||

        {tok_assn,        380}, // =
        {tok_mul_assn,    380}, // *=
        {tok_div_assn,    380}, // /=
        {tok_mod_assn,    380}, // %=
        {tok_add_assn,    380}, // +=
        {tok_sub_assn,    380}, // -=
        {tok_lshift_assn, 380}, // <<=
        {tok_rshift_assn, 380}, // >>=
        {tok_and_assn,    380}, // &=
        {tok_or_assn,     380}, // |=
        {tok_xor_assn,    380}, // ^=
};
