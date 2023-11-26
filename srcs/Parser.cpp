#include "Parser.h"

using namespace std;
using namespace llvm;

Parser::Parser(const string &source_name) : lexer(source_name) {
    advance();
}

void Parser::parse() {
    parse_top_level();
    std::error_code EC;
    raw_fd_ostream output_file{System::output_name, EC};
    if (!System::logger.has_error()) {
        zulctx.module.print(outs(), nullptr);
        zulctx.module.print(output_file, nullptr);
    }
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
    string name;
    while (true) {
        if (cur_tok == tok_eof)
            break;
        if (cur_tok == tok_newline) {
            advance();
        } else if (cur_tok == tok_identifier) { //전역 변수 선언
            parse_global_var();
        } else if (cur_tok == tok_hi) {
            advance();
            if (cur_tok != tok_identifier) {
                lexer.log_cur_token("함수 또는 클래스의 이름이 와야 합니다");
            } else {
                name = lexer.get_word();
                advance();
            }
            if (cur_tok == tok_colon) { //클래스 정의
                cerr << "클래스 정의입니다.\n";
                advance();
            } else if (cur_tok == tok_lpar) { //함수 정의
                advance();
                parse_func_def(name);
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
            lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\"", ". 타입이 와야 합니다"});
            advance();
        }
        auto type_name = lexer.get_word();
        if (type_map.contains(type_name)) {
            if (zulctx.global_var_map.contains(var_name)) {
                lexer.log_cur_token("변수가 다시 정의되었습니다.");
                advance();
                return;
            }
            int type_id = type_map[type_name];
            auto llvm_type = get_llvm_type(zulctx.context, type_id);
            auto init_val = get_llvm_constant(llvm_type, type_id);
            zulctx.global_var_map.emplace(var_name, make_pair(new GlobalVariable(
                    zulctx.module, llvm_type, false, GlobalVariable::ExternalLinkage, init_val, var_name), type_id));
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
        lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
        advance();
    }
}

void Parser::parse_func_def(string &func_name) {
    auto params = parse_parameter();
    int return_type = -1;
    if (cur_tok != tok_identifier) {
        lexer.log_cur_token("아직 반환 타입 자동 추론이 지원되지 않습니다. 반환 타입을 명시해야 합니다");
    } else {
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

    func_proto_map.emplace(func_name, make_unique<FuncProtoAST>(func_name, return_type, std::move(params), zulctx.arg_value_map));

    Token before_tok = cur_tok;
    vector<unique_ptr<AST>> func_body;
    advance();
    while (true) {
        if (cur_tok == tok_eof)
            break;
        if (cur_tok == tok_newline) {
            before_tok = cur_tok;
            advance();
            continue;
        }
        if (before_tok == tok_newline && cur_tok != tok_indent)
            break;
        auto expr = parse_expr_start(zulctx.local_var_map);
        if (expr)
            func_body.push_back(std::move(expr));
        before_tok = cur_tok;
        advance();
    }

    auto llvm_func = func_proto_map[func_name]->code_gen(zulctx);
    auto bb = llvm::BasicBlock::Create(zulctx.context, "entry", llvm_func);
    zulctx.builder.SetInsertPoint(bb);

    for (auto &ast: func_body)
        ast->code_gen(zulctx);

    llvm::verifyFunction(*llvm_func);
    zulctx.local_var_map.clear();
    zulctx.arg_value_map.clear();
}

vector<std::pair<std::string, int>> Parser::parse_parameter() {
    vector<std::pair<std::string, int>> params;
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
            lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\"", ". 콜론이 와야 합니다\""});
        advance();
        if (cur_tok != tok_identifier)
            lexer.log_cur_token("매개변수에는 타입을 명시해야 합니다.");
        else {
            auto type = lexer.get_word();
            if (type_map.contains(type)) {
                params.emplace_back(name, type_map[type]);
            } else {
                lexer.log_cur_token("존재하지 않는 타입입니다.");
            }
        }
        advance();
        if (cur_tok == tok_rpar || cur_tok == tok_eof)
            break;
        if (cur_tok != tok_comma) {
            lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\"", ". 콤마가 와야 합니다\""});
        }
        advance();
    }
    advance();
    return params;
}

unique_ptr<AST> Parser::parse_expr_start(LocalVarMap &local_var_map) {
    unique_ptr<AST> left;
    if (cur_tok == tok_identifier) {
        string name = lexer.get_word();
        auto name_loc = lexer.get_token_start_loc();
        advance();
        if (cur_tok == tok_colon) { //선언만
            advance();
            if (cur_tok != tok_identifier) {
                lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\"", ". 타입이 와야 합니다"});
                advance();
                return nullptr;
            }
            string type = lexer.get_word();
            if (type_map.contains(type)) {
                return make_unique<VariableDeclAST>(Capture(std::move(name), name_loc, name.size()),
                                                    type_map[type], local_var_map);
            } else {
                lexer.log_cur_token("존재하지 않는 타입입니다.");
                advance();
                return nullptr;
            }
        } else if (cur_tok == tok_assn) { //선언 및 대입
            advance();
            unique_ptr<AST> body = parse_expr();
            return make_unique<VariableDeclAST>(Capture(std::move(name), name_loc, name.size()), std::move(body), local_var_map);
        } else if (tok_mul_assn <= cur_tok && cur_tok <= tok_xor_assn) { //나머지 대입 연산
            //만들어야 함
        } else {
            left = parse_identifier(name, name_loc);
        }
    } else {
        left = parse_primary();
    }
    if (!left)
        return nullptr;
    return parse_bin_op(0, std::move(left));
}

unique_ptr<AST> Parser::parse_expr() {
    auto left = parse_primary();
    if (!left)
        return nullptr;
    return parse_bin_op(0, std::move(left));
}

unique_ptr<AST> Parser::parse_bin_op(int prev_prec, unique_ptr<AST> left) {
    while (true) {
        int cur_prec = get_op_prec();

        if (cur_prec < prev_prec) //연산자가 아니면 자연스럽게 리턴함
            return left;

        Token op = cur_tok;
        advance();

        auto right = parse_primary();
        if (!right)
            return nullptr;

        int next_prec = get_op_prec();
        if (cur_prec < next_prec) {
            right = parse_bin_op(cur_prec, std::move(right));
            if (!right)
                return nullptr;
        }
        left = std::make_unique<BinOpAST>(std::move(left), std::move(right), make_capture(op, lexer));
    }
}

unique_ptr<AST> Parser::parse_primary() {
    switch (cur_tok) {
        case tok_identifier:
            return parse_identifier();
        case tok_real:
            return make_unique<ImmRealAST>(stod(lexer.get_word()));
        case tok_int:
            return make_unique<ImmIntAST>(stoll(lexer.get_word()));
        case tok_lpar:
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
    auto loc = lexer.get_token_start_loc();
    advance();
    return parse_identifier(name, loc);
}

unique_ptr<AST> Parser::parse_identifier(string &name, pair<int, int> name_loc) {
    if (cur_tok != tok_lpar) {
        return make_unique<VariableAST>(Capture(std::move(name), name_loc, name.size()));
    }
    if (!func_proto_map.contains(name)) {
        System::logger.log_error(name_loc, name.size(), {"\"", name, "\"은 존재하지 않는 함수입니다"});
        return nullptr;
    }
    advance();
    vector<Capture<unique_ptr<AST>>> args;
    while (true) {
        if (cur_tok == tok_rpar) {
            auto param_cnt = func_proto_map[name]->params.size();
            if (func_proto_map[name]->params.size() != args.size()) {
                lexer.log_cur_token({"인자 개수가 맞지 않습니다. ", "\"", name, "\" 함수의 인자 개수는 ", to_string(param_cnt), "개 입니다."});
                return nullptr;
            }
            advance();
            return make_unique<FuncCallAST>(func_proto_map[name].get(), std::move(args));
        }
        auto arg_start_loc = lexer.get_token_start_loc();
        auto arg = parse_expr();
        if (!arg) return nullptr;
        args.emplace_back(std::move(arg), arg_start_loc, lexer.get_token_start_loc().second - arg_start_loc.second);
        if (cur_tok == tok_comma)
            advance();
        else if (cur_tok != tok_rpar) {
            lexer.log_cur_token("콤마가 필요합니다");
        }
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
