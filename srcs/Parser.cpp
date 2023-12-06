#include "Parser.h"

using namespace std;
using namespace llvm;

Parser::Parser(const string &source_name) : lexer(source_name) {
    zulctx.module->setSourceFileName(source_name);
    zulctx.module->setTargetTriple(sys::getDefaultTargetTriple());

//    zulctx.global_var_map.emplace("ㄲ", make_pair(new GlobalVariable(
//            *zulctx.module, llvm_type, false, GlobalVariable::ExternalLinkage, init_val, "ㄲ"), 5));
    advance();
}

void Parser::parse() {
    parse_top_level();
    if (!System::logger.has_error()) {
        std::error_code EC;
        raw_fd_ostream output_file{System::output_name, EC};
        zulctx.module->print(output_file, nullptr);
    }
}

ZulContext &Parser::get_zulctx() {
    return zulctx;
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
    pair<int, int> name_loc;
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
                name_loc = lexer.get_token_start_loc();
                advance();
            }
            if (cur_tok == tok_colon) { //클래스 정의
                cerr << "클래스 정의입니다.\n";
                advance();
            } else if (cur_tok == tok_lpar) { //함수 정의
                advance();
                if (func_proto_map.contains(name) && func_proto_map[name].has_body) {
                    System::logger.log_error(name_loc, name.size(), {"\"", name, "\" 함수는 이미 정의된 함수입니다."});
                    advance();
                } else {
                    parse_func_def(name, name_loc, 1);
                }
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
            auto llvm_type = get_llvm_type(*zulctx.context, type_id);
            auto init_val = get_const_zero(llvm_type, type_id);
            //GlobalVariable 소멸자 호출되면 dropAllReferences 때문에 에러남. 어쩔 수 없이 그냥 동적할당 사용.
            zulctx.global_var_map.emplace(var_name, make_pair(new GlobalVariable(
                    *zulctx.module, llvm_type, false, GlobalVariable::ExternalLinkage, init_val, var_name), type_id));
            advance();
        } else {
            lexer.log_cur_token("존재하지 않는 타입입니다");
            advance();
        }

    } else if (cur_tok == tok_assn) { //선언과 초기화
        lexer.log_cur_token("아직 전역변수 초기화가 지원되지 않습니다");
        while (cur_tok != tok_newline && cur_tok != tok_eof)
            advance();
    } else {
        lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
        advance();
    }
}

pair<vector<pair<string, int>>, bool> Parser::parse_parameter() {
    vector<pair<string, int>> params;
    if (cur_tok == tok_rpar) {
        advance();
        return {params, false};
    }
    while (true) {
        if (cur_tok == tok_va_arg) {
            advance();
            if (cur_tok != tok_rpar) {
                lexer.log_cur_token("가변 인자 뒤에는 다른 인자가 올 수 없습니다");
            }
            advance();
            return {params, true};
        }
        if (cur_tok != tok_identifier)
            lexer.log_cur_token("함수의 매개변수가 와야 합니다");
        auto name = lexer.get_word();
        if (type_map.contains(name)) { //타입만 명시
            params.emplace_back("", type_map[name]);
        } else {
            advance();
            if (cur_tok != tok_colon)
                lexer.log_cur_token("콜론이 와야 합니다");
            advance();
            if (cur_tok == tok_identifier) {
                auto type = lexer.get_word();
                if (type_map.contains(type)) {
                    params.emplace_back(name, type_map[type]);
                    zulctx.local_var_map.emplace(name, make_pair(nullptr, -1));
                } else {
                    lexer.log_cur_token("존재하지 않는 타입입니다.");
                }
            } else {
                lexer.log_cur_token("매개변수에는 타입을 명시해야 합니다.");
            }
        }
        advance();
        if (cur_tok == tok_rpar || cur_tok == tok_eof)
            break;
        if (cur_tok != tok_comma) {
            lexer.log_cur_token("콤마가 와야 합니다");
        }
        advance();
    }
    advance();
    return {params, false};
}

void Parser::parse_func_def(string &func_name, pair<int, int> name_loc, int target_level) {
//---------------------------------함수 프로토타입 파싱---------------------------------
    auto [params, is_var_arg] = parse_parameter();
    cur_func_ret_type = -1;
    if (cur_tok == tok_identifier) {
        auto type_name = lexer.get_word();
        if (type_map.contains(type_name)) {
            cur_func_ret_type = type_map[type_name];
        } else {
            lexer.log_cur_token({"\"", type_name, "\" 는 존재하지 않는 타입입니다."});
        }
        advance();
    }
    if (func_name == ENTRY_FN_NAME && cur_func_ret_type != 2) {
        System::logger.log_error(name_loc, func_name.size(), {ENTRY_FN_NAME, " 함수의 반환 타입은 반드시 \"수\" 여야 합니다"});
    }
    if (cur_tok == tok_newline) { //함수 선언만
        func_proto_map.emplace(func_name,
                               FuncProtoAST(func_name, cur_func_ret_type, std::move(params), false, is_var_arg));
        func_proto_map[func_name].code_gen(zulctx);
        zulctx.local_var_map.clear();
        return;
    }
    if (cur_tok != tok_colon)
        lexer.log_cur_token("콜론이 와야 합니다");
    advance();
    if (cur_tok != tok_newline)
        lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
    advance();
    func_proto_map.emplace(func_name, FuncProtoAST(func_name, cur_func_ret_type, std::move(params), true, is_var_arg));
//---------------------------------함수 몸체 파싱---------------------------------
    auto [func_body, stop_level] = parse_block_body(target_level);
    if (func_body.empty() && !System::logger.has_error()) {
        System::logger.log_error(name_loc, func_name.size(),
                                 "함수의 몸체가 정의되지 않았습니다\n(함수 선언만 하기 위해선 콜론을 사용하지 않아야 합니다)");
        return;
    }
    create_func(func_proto_map[func_name], func_body, name_loc);
}

std::pair<ASTPtr, int> Parser::parse_line(int start_level, int target_level) {
    int level = start_level;
    while (level < target_level && cur_tok == tok_indent) {
        advance();
        level++;
    }
    if (level < target_level) {
        return {nullptr, level};
    }
    if (level == target_level && cur_tok == tok_indent) {
        lexer.log_cur_token("들여쓰기 깊이가 올바르지 않습니다");
        while (cur_tok == tok_indent)
            advance();
    }
    ASTPtr ret;
    if (cur_tok == tok_go) { //ㄱㄱ문
        advance();
        return parse_for(target_level + 1);
    } else if (cur_tok == tok_ij) { //ㅇㅈ?문
        advance();
        return parse_if(target_level + 1);
    } else if (cur_tok == tok_gg) { //ㅈㅈ문
        zulctx.ret_count++;
        auto cap = make_capture(cur_func_ret_type, lexer);
        advance();
        auto body = parse_expr();
        ret = make_unique<FuncRetAST>(std::move(body), std::move(cap));
    } else if (cur_tok == tok_tt) { //ㅌㅌ
        if (!zulctx.in_loop) {
            lexer.log_cur_token("ㅌㅌ문을 사용할 수 없습니다. 루프가 아닙니다");
            return {nullptr, -1};
        }
        advance();
        ret = make_unique<ContinueAST>();
    } else if (cur_tok == tok_sg) { //ㅅㄱ
        if (!zulctx.in_loop) {
            lexer.log_cur_token("ㅅㄱ문을 사용할 수 없습니다. 루프가 아닙니다");
            return {nullptr, -1};
        }
        advance();
        ret = make_unique<BreakAST>();
    } else {
        ret = parse_expr_start();
    }
    if (cur_tok != tok_newline && cur_tok != tok_eof) {
        lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\""});
        while (cur_tok != tok_newline && cur_tok != tok_eof)
            advance();
    }
    advance();
    return {std::move(ret), -1};
}

ASTPtr Parser::parse_expr_start() {
    ASTPtr left;
    if (cur_tok == tok_identifier) {
        string name = lexer.get_word();
        auto name_loc = lexer.get_token_start_loc();
        advance();
        if (cur_tok == tok_colon || (tok_assn <= cur_tok && cur_tok <= tok_xor_assn)) {
            return parse_local_var(name, name_loc);
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

ASTPtr Parser::parse_local_var(string &name, pair<int, int> name_loc) {
    bool is_exist = zulctx.global_var_map.contains(name) || zulctx.local_var_map.contains(name);
    auto op_cap = make_capture(cur_tok, lexer);
    auto name_cap = Capture(std::move(name), name_loc, name.size());
    if (op_cap.value == tok_colon) { //선언
        advance();
        if (cur_tok != tok_identifier) {
            lexer.log_cur_token("타입이 와야 합니다");
            advance();
            return nullptr;
        }
        string type = lexer.get_word();
        auto loc = lexer.get_token_start_loc();
        advance();
        if (!type_map.contains(type)) {
            System::logger.log_error(loc, type.size(), "존재하지 않는 타입입니다.");
            return nullptr;
        } else if (is_exist) {
            System::logger.log_error(name_loc, name_cap.word_size, "변수가 재정의되었습니다");
            return nullptr;
        }
        if (cur_tok == tok_assn) { //선언 + 초기화
            advance();
            ASTPtr body = parse_expr();
            if (!body)
                return nullptr;
            return make_unique<VariableDeclAST>(std::move(name_cap), type_map[type], std::move(body), zulctx);
        }
        return make_unique<VariableDeclAST>(std::move(name_cap), type_map[type], zulctx);
    } else { //자동추론 + 초기화
        advance();
        ASTPtr body = parse_expr();
        if (!body)
            return nullptr;
        if (!is_exist) {
            if (op_cap.value == tok_assn) {
                return make_unique<VariableDeclAST>(std::move(name_cap), std::move(body), zulctx);
            } else {
                System::logger.log_error(name_loc, name_cap.word_size, {"\"", name_cap.value, "\" 는 존재하지 않는 변수입니다"});
                return nullptr;
            }
        }
        return make_unique<VariableAssnAST>(make_unique<VariableAST>(std::move(name_cap.value)), std::move(op_cap),
                                            std::move(body));
    }
}

ASTPtr Parser::parse_expr() {
    auto left = parse_primary();
    if (!left)
        return nullptr;
    return parse_bin_op(0, std::move(left));
}

ASTPtr Parser::parse_bin_op(int prev_prec, ASTPtr left) {
    while (true) {
        int cur_prec = get_op_prec();

        if (cur_prec < prev_prec) //연산자가 아니면 자연스럽게 리턴함
            return left;
        if (cur_prec == op_prec_map[tok_assn]) {
            lexer.log_cur_token("하나의 식에는 하나의 대입 연산자만 사용할 수 있습니다");
            while (cur_tok != tok_newline)
                advance();
            return left;
        }

        auto op_cap = make_capture(cur_tok, lexer);
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
        left = std::make_unique<BinOpAST>(std::move(left), std::move(right), std::move(op_cap));
    }
}

ASTPtr Parser::parse_primary() {
    double dval;
    long long llval;
    switch (cur_tok) {
        case tok_identifier:
            return parse_identifier();
        case tok_real:
            dval = stod(lexer.get_word());
            advance();
            return make_unique<ImmRealAST>(dval);
        case tok_int:
            llval = stoll(lexer.get_word());
            advance();
            return make_unique<ImmIntAST>(llval);
        case tok_lpar:
            return parse_par();
        case tok_add:
        case tok_sub:
        case tok_not:
        case tok_bitnot:
            return parse_unary_op();
        case tok_dquotes:
            return parse_str();
        case tok_squotes:
            return parse_char();
        default:
            return nullptr;
    }
}

std::pair<std::vector<ASTPtr>, int> Parser::parse_block_body(int target_level) {
    vector<ASTPtr> body;
    int start_level = 0;
    while (true) {
        if (cur_tok == tok_newline) {
            advance();
            continue;
        }
        auto [parsed_expr, stop_level] = parse_line(start_level, target_level);
        if (parsed_expr)
            body.push_back(std::move(parsed_expr));
        if (stop_level == -1) {
            start_level = 0;
        } else if (stop_level < target_level) {
            return {std::move(body), stop_level};
        } else {
            start_level = stop_level;
        }
    }
}

pair<ASTPtr, bool> Parser::parse_if_header() {
    ASTPtr cond;
    bool error = false;
    cond = parse_expr();
    if (!cond) {
        lexer.log_cur_token("조건식이 필요합니다");
        error = true;
    }
    if (cur_tok != tok_colon) {
        lexer.log_cur_token("콜론이 필요합니다");
        error = true;
    }
    advance();
    return {std::move(cond), error};
}

std::pair<ASTPtr, int> Parser::parse_if(int target_level) {
    CondBodyPair if_pair;
    std::vector<CondBodyPair> elif_pair_list;
    std::vector<ASTPtr> else_body;
//---------------------------------if문 파싱---------------------------------
    zulctx.scope_stack.emplace();
    auto [if_cond, error] = parse_if_header();
    auto [if_body, stop_level] = parse_block_body(target_level);
    zulctx.remove_top_scope_vars();
    if (if_body.empty() && !System::logger.has_error()) {
        lexer.log_cur_token("ㅇㅈ?문의 몸체가 정의되지 않았습니다");
        error = true;
    }
    if_pair = {std::move(if_cond), std::move(if_body)};
//---------------------------------elif문 파싱---------------------------------
    while (stop_level == target_level - 1 && cur_tok == tok_no) {
        zulctx.scope_stack.emplace();
        advance();
        auto [elif_cond, elif_err] = parse_if_header();
        auto [elif_body, level] = parse_block_body(target_level);
        zulctx.remove_top_scope_vars();
        stop_level = level;
        error = error || elif_err;
        if (elif_body.empty() && !System::logger.has_error()) {
            lexer.log_cur_token("ㄴㄴ?문의 몸체가 정의되지 않았습니다");
            error = true;
        }
        elif_pair_list.emplace_back(std::move(elif_cond), std::move(elif_body));
    }
//---------------------------------else문 파싱---------------------------------
    if (stop_level == target_level - 1 && cur_tok == tok_nope) {
        advance();
        zulctx.scope_stack.emplace();
        if (cur_tok != tok_colon) {
            lexer.log_cur_token("콜론이 필요합니다");
            error = true;
        }
        advance();
        auto [body, level] = parse_block_body(target_level);
        zulctx.remove_top_scope_vars();
        stop_level = level;
        if (body.empty() && !System::logger.has_error()) {
            lexer.log_cur_token("ㄴㄴ문의 몸체가 정의되지 않았습니다");
            error = true;
        }
        else_body = std::move(body);
    }
    if (error)
        return {nullptr, stop_level};
    return {make_unique<IfAST>(std::move(if_pair), std::move(elif_pair_list), std::move(else_body)), stop_level};
}

std::pair<ASTPtr, int> Parser::parse_for(int target_level) {
    ASTPtr init_for = nullptr;
    ASTPtr test_for = nullptr;
    ASTPtr update_for = nullptr;
    zulctx.scope_stack.emplace(); //스코프 등록
//---------------------------------for문 헤더 파싱---------------------------------
    auto expr = parse_expr_start();
    if (cur_tok == tok_semicolon) {
        init_for = std::move(expr);
        advance();
        test_for = parse_expr_start();
        if (cur_tok != tok_semicolon) {
            lexer.log_cur_token("ㄱㄱ문에는 세미콜론이 아예 오지 않거나, 2개가 와야 합니다. 세미콜론이 필요합니다");
        }
        advance();
        update_for = parse_expr_start();
    } else if (expr) {
        test_for = std::move(expr);
    }
    if (cur_tok != tok_colon) {
        lexer.log_cur_token({"예기치 않은 토큰 \"", lexer.get_word(), "\" 콜론이 와야 합니다"});
    }
    advance();
//---------------------------------for문 몸체 파싱---------------------------------
    zulctx.in_loop = true;
    auto [for_body, stop_level] = parse_block_body(target_level);
    zulctx.in_loop = false;
    zulctx.remove_top_scope_vars();
    if (for_body.empty() && !System::logger.has_error()) {
        lexer.log_cur_token("ㄱㄱ문의 몸체가 정의되지 않았습니다");
        return {nullptr, stop_level};
    }
    return {make_unique<LoopAST>(std::move(init_for), std::move(test_for), std::move(update_for),
                                 std::move(for_body)), stop_level};
}

ASTPtr Parser::parse_identifier() {
    auto name = lexer.get_word();
    auto loc = lexer.get_token_start_loc();
    advance();
    return parse_identifier(name, loc);
}

ASTPtr Parser::parse_identifier(string &name, pair<int, int> name_loc) {
    if (cur_tok != tok_lpar) {
        if (!zulctx.local_var_map.contains(name) && !zulctx.global_var_map.contains(name)) {
            System::logger.log_error(name_loc, name.size(), {"\"", name, "\" 는 존재하지 않는 변수입니다"});
            return nullptr;
        }
        return make_unique<VariableAST>(name);
    }
    if (!func_proto_map.contains(name)) {
        System::logger.log_error(name_loc, name.size(), {"\"", name, "\" 는 존재하지 않는 함수입니다"});
        return nullptr;
    }
    advance();
    vector<Capture<ASTPtr>> args;
    while (true) {
        if (cur_tok == tok_rpar) {
            auto &proto = func_proto_map[name];
            auto param_cnt = proto.params.size();
            if (!proto.is_var_arg && param_cnt != args.size()) {
                lexer.log_cur_token({"인자 개수가 맞지 않습니다. ", "\"", name, "\" 함수의 인자 개수는 ", to_string(param_cnt), "개 입니다."});
                advance();
                return nullptr;
            }
            advance();
            return make_unique<FuncCallAST>(func_proto_map[name], std::move(args));
        }
        auto arg_start_loc = lexer.get_token_start_loc();
        auto arg = parse_expr();
        if (!arg)
            return nullptr;
        args.emplace_back(std::move(arg), arg_start_loc, lexer.get_token_start_loc().second - arg_start_loc.second);
        if (cur_tok == tok_comma)
            advance();
        else if (cur_tok != tok_rpar) {
            lexer.log_cur_token("콤마가 필요합니다");
        }
    }
}

ASTPtr Parser::parse_unary_op() {
    auto op_cap = make_capture(cur_tok, lexer);
    advance();

    auto body = parse_primary();
    if (!body)
        return nullptr;
    return make_unique<UnaryOpAST>(std::move(body), std::move(op_cap));
}

ASTPtr Parser::parse_par() {
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

ASTPtr Parser::parse_str() {
    int st = lexer.get_line_index() + 1;
    do {
        advance();
        if (cur_tok == tok_newline || cur_tok == tok_eof) {
            lexer.log_cur_token("쌍따옴표가 닫히지 않았습니다. \"가 필요합니다");
            return nullptr;
        }
    } while (cur_tok != tok_dquotes);
    int ed = lexer.get_line_index();
    auto str = lexer.get_line_substr(st, ed);
    stringstream ss;
    for (int i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size() && str[i + 1] == 'n') {
            ss << '\n';
            i++;
        } else {
            ss << str[i];
        }
    }
    advance();
    return make_unique<ImmStrAST>(ss.str());
}

ASTPtr Parser::parse_char() {
    int st = lexer.get_line_index() + 1;
    do {
        advance();
        if (cur_tok == tok_newline || cur_tok == tok_eof) {
            lexer.log_cur_token("따옴표가 닫히지 않았습니다. \'가 필요합니다");
            return nullptr;
        }
    } while (cur_tok != tok_squotes);
    int ed = lexer.get_line_index();
    auto str = lexer.get_line_substr(st, ed);
    advance();
    if (str.size() > 1) {
        lexer.log_cur_token("\"글자\" 자료형은 1바이트입니다. 자료형의 범위를 초과합니다");
        return nullptr;
    } else if (str.empty()) {
        lexer.log_cur_token("빈 글자는 존재할 수 없습니다");
        return nullptr;
    }
    return make_unique<ImmCharAST>(str[0]);
}

void Parser::create_func(FuncProtoAST &proto, const vector<ASTPtr> &body, std::pair<int, int> name_loc) {
    auto llvm_func = proto.code_gen(zulctx);
    auto entry_block = BasicBlock::Create(*zulctx.context, "entry", llvm_func);
    zulctx.builder.SetInsertPoint(entry_block);

    if (zulctx.ret_count > 1) {
        if (proto.return_type != -1)
            zulctx.return_var = zulctx.builder.CreateAlloca(get_llvm_type(*zulctx.context, proto.return_type), nullptr,
                                                            "ret");
        zulctx.return_block = BasicBlock::Create(*zulctx.context, "func_ret");
    }

    int i = 0;
    for (auto &arg: llvm_func->args()) {
        auto alloca_val = zulctx.builder.CreateAlloca(arg.getType(), nullptr, arg.getName());
        zulctx.builder.CreateStore(&arg, alloca_val);
        zulctx.local_var_map[proto.params[i].first] = make_pair(alloca_val, proto.params[i].second);
        i++;
    }

    for (auto &ast: body) {
        auto code = ast->code_gen(zulctx).second;
        if (code == -10)
            break;
    }

    if (zulctx.builder.GetInsertBlock()->empty() || zulctx.ret_count == 0) {
        if (proto.name == ENTRY_FN_NAME) {
            if (zulctx.ret_count > 1)
                zulctx.builder.CreateBr(zulctx.return_block);
            else
                zulctx.builder.CreateRet(ConstantInt::get(Type::getInt64Ty(*zulctx.context), 0, true));
        } else if (proto.return_type == -1) {
            zulctx.builder.CreateRetVoid();
        } else {
            System::logger.log_error(name_loc, proto.name.size(),
                                     {"\"", proto.name, "\" 함수의 리턴 타입은 \"", type_name_map[proto.return_type],
                                      "\" 입니다. ㅈㅈ구문이 필요합니다"});
        }
    }
    if (zulctx.ret_count > 1) {
        llvm_func->insert(llvm_func->end(), zulctx.return_block);
        zulctx.builder.SetInsertPoint(zulctx.return_block);
        if (proto.return_type == -1) {
            zulctx.builder.CreateRetVoid();
        } else {
            auto ret = zulctx.builder.CreateLoad(zulctx.return_var->getAllocatedType(), zulctx.return_var);
            zulctx.builder.CreateRet(ret);
        }
    }
    verifyFunction(*llvm_func);
    zulctx.local_var_map.clear();
    zulctx.ret_count = 0;
}

std::unordered_map<Token, int> Parser::op_prec_map = {
        {tok_bitnot,      120}, // ~
        {tok_not,         120}, // !

        {tok_mul,         110}, // *
        {tok_div,         110}, // /
        {tok_mod,         110}, // %

        {tok_add,         100}, // +
        {tok_sub,         100}, // -

        {tok_lshift,      90}, // <<
        {tok_rshift,      90}, // >>

        {tok_lt,          80}, // <
        {tok_gt,          80}, // >
        {tok_lteq,        80}, // <=
        {tok_gteq,        80}, // >=

        {tok_eq,          70}, // ==
        {tok_ineq,        70}, // !=

        {tok_bitand,      60}, // &

        {tok_bitxor,      50}, // ^

        {tok_bitor,       40}, // |

        {tok_and,         30}, // &&

        {tok_or,          20}, // ||

        {tok_assn,        10}, // =
        {tok_mul_assn,    10}, // *=
        {tok_div_assn,    10}, // /=
        {tok_mod_assn,    10}, // %=
        {tok_add_assn,    10}, // +=
        {tok_sub_assn,    10}, // -=
        {tok_lshift_assn, 10}, // <<=
        {tok_rshift_assn, 10}, // >>=
        {tok_and_assn,    10}, // &=
        {tok_or_assn,     10}, // |=
        {tok_xor_assn,    10}, // ^=
};
