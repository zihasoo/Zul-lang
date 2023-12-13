
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Parser.h"

using std::string;
using std::unique_ptr;
using std::pair;
using std::tuple;
using std::vector;
using std::cerr;
using std::stringstream;
using std::make_unique;
using std::make_pair;
using std::to_string;

using llvm::BranchInst;
using llvm::ReturnInst;
using llvm::isa;
using llvm::Module;
using llvm::LLVMContext;
using llvm::BasicBlock;
using llvm::Type;
using llvm::Value;
using llvm::ArrayType;
using llvm::PointerType;
using llvm::GlobalVariable;
using llvm::Function;
using llvm::Constant;
using llvm::ConstantInt;
using llvm::ConstantAggregateZero;
using llvm::sys::getDefaultTargetTriple;

bool remove_all_pred(BasicBlock *bb) {
    if (bb->isEntryBlock())
        return false;
    if (bb->hasNPredecessors(0)) {
        bb->dropAllReferences();
        bb->eraseFromParent();
        return true;
    }
    auto pred = bb->getSinglePredecessor();
    if (!pred)
        return false;
    if (remove_all_pred(pred)) {
        bb->dropAllReferences();
        bb->eraseFromParent();
        return true;
    }
    return false;
}

Parser::Parser(const string &source_name) : lexer(source_name) {
    zulctx.module->setSourceFileName(source_name);
    zulctx.module->setTargetTriple(getDefaultTargetTriple());
    advance();
}

pair<unique_ptr<llvm::LLVMContext>, unique_ptr<llvm::Module>> Parser::parse() {
    auto i32ty = Type::getInt32Ty(*zulctx.context);
    auto i8ptrty = PointerType::getUnqual(Type::getInt8Ty(*zulctx.context));
    auto fty = llvm::FunctionType::get(i32ty, {i8ptrty}, true);

    llvm::Function::Create(fty, llvm::Function::ExternalLinkage, "printf", *zulctx.module);
    llvm::Function::Create(fty, llvm::Function::ExternalLinkage, "scanf", *zulctx.module);

    parse_top_level();

    if (!func_proto_map.contains(ENTRY_FN_NAME) || !func_proto_map[ENTRY_FN_NAME].has_body) {
        cerr << "에러: 진입점이 정의되지 않았습니다. \"" << ENTRY_FN_NAME << "\" 함수 정의가 필요합니다\n";
        System::logger.set_error();
    }

    return {std::move(zulctx.context), std::move(zulctx.module)};
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
            System::logger.flush();
            advance();
        } else if (cur_tok == tok_identifier) { //전역 변수 선언
            parse_global_var();
        } else if (cur_tok == tok_hi) {
            advance();
            if (cur_tok != tok_identifier) {
                lexer.log_unexpected("함수 또는 클래스의 이름이 와야 합니다");
            } else {
                name = lexer.get_word();
                name_loc = lexer.get_token_loc();
                advance();
            }
            if (cur_tok == tok_colon) { //클래스 정의
                lexer.log_token("아직 클래스 정의가 지원되지 않습니다");
                advance();
            } else if (cur_tok == tok_lpar) { //함수 정의
                advance();
                parse_func_def(name, name_loc, 1);
            } else {
                lexer.log_unexpected();
                advance();
            }
        } else {
            lexer.log_unexpected();
            advance();
        }
    }
}

void Parser::parse_global_var() {
    auto var_name = lexer.get_word();
    auto var_loc = lexer.get_token_loc();
    advance();
    if (zulctx.global_var_map.contains(var_name)) {
        System::logger.log_error(var_loc, var_name.size(), "변수가 다시 정의되었습니다.");
        return;
    }
    if (cur_tok == tok_colon) { //타입 명시
        advance();
        auto [type_id, size_expr] = parse_type();
        if (type_id == -1)
            return;
        if (cur_tok == tok_assn) { //선언과 초기화
            advance();
            auto body = parse_expr();
            if (!body)
                return;
            if (!body->is_const()) {
                System::logger.log_error(var_loc, var_name.size(), "대입 구문이 상수식이 아닙니다. 전역 변수는 상수식으로만 초기화 할 수 있습니다.");
                return;
            }
            if (size_expr != nullptr) {
                System::logger.log_error(var_loc, var_name.size(), "배열 타입은 아직 선언과 동시에 초기화 할 수 없습니다");
                return;
            }
            auto init_val = body->code_gen(zulctx);
            if (init_val.second != type_id && !create_cast(zulctx, init_val, type_id)) {
                System::logger.log_error(var_loc, var_name.size(),
                                         {"대입 연산식의 타입 \"",
                                          get_type_name(init_val.second), "\" 에서 변수의 타입 \"",
                                          get_type_name(type_id),
                                          "\" 로 캐스팅 할 수 없습니다"});
                return;
            }
            auto global_var = new GlobalVariable(*zulctx.module, init_val.first->getType(), false,
                                                 GlobalVariable::ExternalLinkage,
                                                 static_cast<Constant *>(init_val.first), var_name);
            zulctx.global_var_map.emplace(var_name, make_pair(global_var, type_id));
            return;
        }
        //선언만
        auto llvm_type = get_llvm_type(*zulctx.context, type_id);
        if (size_expr == nullptr) { //배열이 아닌 경우
            auto init_val = get_const_zero(llvm_type, type_id);
            //GlobalVariable 소멸자 호출되면 dropAllReferences 때문에 에러남. 그냥 동적 할당 해야됨
            auto global_var = new GlobalVariable(*zulctx.module, llvm_type, false, GlobalVariable::ExternalLinkage,
                                                 init_val, var_name);
            zulctx.global_var_map.emplace(var_name, make_pair(global_var, type_id));
            return;
        }
        //배열인 경우
        if (size_expr->is_const()) {
            auto size_val = size_expr->code_gen(zulctx);
            if (size_val.second == id_int) {
                int arr_size = static_cast<ConstantInt *>(size_val.first)->getSExtValue();
                if (arr_size > 0) {
                    auto arr_type = ArrayType::get(llvm_type, arr_size);
                    auto global_var = new GlobalVariable(*zulctx.module, arr_type, false,
                                                         GlobalVariable::ExternalLinkage,
                                                         ConstantAggregateZero::get(arr_type), var_name);
                    zulctx.global_var_map.emplace(var_name, make_pair(global_var, type_id));
                    return;
                }
            }
        }
        System::logger.log_error(var_loc, var_name.size(), "배열 크기는 0이 아닌 상수 정수여야 합니다");

    } else if (cur_tok == tok_assn) { //타입 추론 및 초기화
        advance();
        auto body = parse_expr();
        if (!body)
            return;
        if (!body->is_const()) {
            System::logger.log_error(var_loc, var_name.size(), "대입 구문이 상수식이 아닙니다. 전역 변수는 상수식으로만 초기화 할 수 있습니다.");
            return;
        }
        auto init_val = body->code_gen(zulctx);
        GlobalVariable *global_var;
        if (init_val.second == id_char + TYPE_COUNTS) {
            global_var = static_cast<GlobalVariable *>(init_val.first);
        } else {
            global_var = new GlobalVariable(*zulctx.module, init_val.first->getType(), false,
                                            GlobalVariable::ExternalLinkage,
                                            static_cast<Constant *>(init_val.first), var_name);
        }
        zulctx.global_var_map.emplace(var_name, make_pair(global_var, init_val.second));
    } else {
        lexer.log_unexpected();
    }
}

tuple<vector<pair<string, int>>, bool, bool> Parser::parse_parameter() {
    vector<pair<string, int>> params;
    bool err = false;
    if (cur_tok == tok_rpar) {
        advance();
        return {params, false, err};
    }
    while (true) {
        if (cur_tok == tok_va_arg) {
            advance();
            if (cur_tok != tok_rpar) {
                lexer.log_token("가변 인자 뒤에는 다른 인자가 올 수 없습니다. )가 와야 합니다");
                err = true;
                while (cur_tok != tok_rpar && cur_tok != tok_eof && cur_tok != tok_newline)
                    advance();
            }
            advance();
            return {params, true, err};
        }
        if (cur_tok != tok_identifier) {
            lexer.log_unexpected("함수의 매개변수가 와야 합니다");
            err = true;
        }
        auto name = lexer.get_word();
        if (type_map.contains(name)) { //타입만 명시
            auto type = parse_type(true);
            params.emplace_back("", type.first);
        } else if (zulctx.global_var_map.contains(name)) {
            lexer.log_token("이미 존재하는 변수명을 함수 매개변수로 사용할 수 없습니다");
            err = true;
            while (cur_tok != tok_comma && cur_tok != tok_rpar && cur_tok != tok_eof)
                advance();
        } else {
            advance();
            if (cur_tok != tok_colon) {
                lexer.log_unexpected("콜론이 와야 합니다");
                err = true;
            }
            advance();
            auto type = parse_type(true);
            params.emplace_back(name, type.first);
            zulctx.local_var_map.emplace(name, make_pair(nullptr, type.first));
        }
        if (cur_tok == tok_rpar)
            break;
        if (cur_tok == tok_eof || cur_tok == tok_newline) {
            lexer.log_unexpected("괄호가 닫히지 않았습니다. )가 와야 합니다");
            return {params, false, true};
        }
        if (cur_tok != tok_comma) {
            lexer.log_unexpected("콤마가 와야 합니다");
            err = true;
        }
        advance();
    }
    advance();
    return {params, false, err};
}

void Parser::parse_func_def(string &func_name, pair<int, int> name_loc, int target_level) {
//---------------------------------전방 선언된 함수인지 확인---------------------------------
    bool exist = false;
    if (func_proto_map.contains(func_name)) {
        exist = true;
        if (func_proto_map[func_name].has_body) {
            System::logger.log_error(name_loc, func_name.size(), {"\"", func_name, "\" 함수는 이미 정의된 함수입니다."});
            Token before = cur_tok;
            while (true) {
                if (cur_tok == tok_eof || (before == tok_newline && cur_tok != tok_indent))
                    break;
                before = cur_tok;
                advance();
            }
            return;
        }
    }
//---------------------------------함수 프로토타입 파싱---------------------------------
    auto [params, is_var_arg, err] = parse_parameter();
    if (exist && !err) { //전방 선언된 함수면 프로토타입이 같은지 확인
        auto &origin_params = func_proto_map[func_name].params;
        if (params.size() != origin_params.size() || func_proto_map[func_name].is_var_arg != is_var_arg) {
            System::logger.log_error(name_loc, func_name.size(), "전방 선언된 함수와 매개변수 개수가 맞지 않습니다");
            err = true;
        } else {
            for (int i = 0; i < origin_params.size(); ++i) {
                if (origin_params[i].second != params[i].second) {
                    System::logger.log_error(name_loc, func_name.size(),
                                             {"전방 선언된 함수와 매개변수의 타입이 일치하지 않습니다. 전방 선언된 함수의 매개변수 타입은 \"",
                                              get_type_name(origin_params[i].second),
                                              "\" 이고, 정의된 타입은 \"", get_type_name(params[i].second), "\" 입니다"});
                    err = true;
                }
            }
            if (!err) {
                origin_params = std::move(params);
            }
        }
    }
    cur_ret_type = -1;
    if (cur_tok == tok_identifier) {
        auto type_name = lexer.get_word();
        if (type_map.contains(type_name)) {
            cur_ret_type = type_map[type_name];
        } else {
            lexer.log_token({"\"", type_name, "\" 는 존재하지 않는 타입입니다."});
            err = true;
        }
        advance();
    }
    if (exist && func_proto_map[func_name].return_type != cur_ret_type) {
        System::logger.log_error(name_loc, func_name.size(), {"전방 선언된 함수와 반환 타입이 일치하지 않습니다. 전방 선언된 함수의 리턴 타입은 \"",
                                                              get_type_name(func_proto_map[func_name].return_type),
                                                              "\" 이고, 정의된 리턴 타입은 \"", get_type_name(cur_ret_type),
                                                              "\" 입니다"});
        err = true;
    }
    if (func_name == ENTRY_FN_NAME && cur_ret_type != id_int) {
        System::logger.log_error(name_loc, func_name.size(), {ENTRY_FN_NAME, " 함수의 반환 타입은 반드시 \"수\" 여야 합니다"});
        err = true;
    }
    if (cur_tok == tok_newline) { //함수 선언만
        if (exist) {
            System::logger.log_error(name_loc, func_name.size(), "이미 선언된 함수를 다시 선언할 수 없습니다");
        } else if (!err) {
            func_proto_map.emplace(func_name,
                                   FuncProtoAST(func_name, cur_ret_type, std::move(params), false, is_var_arg));
            func_proto_map[func_name].code_gen(zulctx);
        }
        zulctx.local_var_map.clear();
        advance();
        return;
    }
    if (cur_tok != tok_colon) {
        lexer.log_unexpected("콜론이 와야 합니다");
        err = true;
    }
    advance();
    if (cur_tok != tok_newline) {
        lexer.log_unexpected();
        err = true;
    }
    advance();
    if (!err) {
        if (exist) {
            func_proto_map[func_name].has_body = true;
        } else {
            func_proto_map.emplace(func_name,
                                   FuncProtoAST(func_name, cur_ret_type, std::move(params), true, is_var_arg));
        }
    }
//---------------------------------함수 몸체 파싱---------------------------------
    auto [func_body, stop_level] = parse_block_body(target_level);
    if (func_body.empty() && !System::logger.has_error()) {
        System::logger.log_error(name_loc, func_name.size(),
                                 "함수의 몸체가 정의되지 않았습니다. 함수 선언만 하기 위해선 콜론을 사용하지 않아야 합니다");
        return;
    }
    if (!err) {
        create_func(func_proto_map[func_name], func_body, name_loc, exist);
    }
}

pair<vector<ASTPtr>, int> Parser::parse_block_body(int target_level) {
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

std::pair<ASTPtr, int> Parser::parse_line(int start_level, int target_level) {
    int level = start_level;
    while (level < target_level && cur_tok == tok_indent) {
        advance();
        level++;
    }
    if (level < target_level) {
        return {nullptr, level};
    }
    if (cur_tok == tok_indent) {
        while (cur_tok == tok_indent)
            advance();
        if (cur_tok != tok_newline && cur_tok != tok_eof)
            lexer.log_token("들여쓰기 깊이가 올바르지 않습니다");
    }
    ASTPtr ret;
    if (cur_tok == tok_go) { //ㄱㄱ문
        return parse_for(target_level + 1);
    } else if (cur_tok == tok_ij) { //ㅇㅈ?문
        return parse_if(target_level + 1);
    } else if (cur_tok == tok_gg) { //ㅈㅈ문
        zulctx.ret_count++;
        auto cap = make_capture(cur_ret_type, lexer);
        advance();
        auto body = parse_expr();
        ret = make_unique<FuncRetAST>(std::move(body), std::move(cap));
    } else if (cur_tok == tok_tt) { //ㅌㅌ
        if (!zulctx.in_loop) {
            lexer.log_token("ㅌㅌ문을 사용할 수 없습니다. 루프가 아닙니다");
            return {nullptr, -1};
        }
        advance();
        ret = make_unique<ContinueAST>();
    } else if (cur_tok == tok_sg) { //ㅅㄱ
        if (!zulctx.in_loop) {
            lexer.log_token("ㅅㄱ문을 사용할 수 없습니다. 루프가 아닙니다");
            return {nullptr, -1};
        }
        advance();
        ret = make_unique<BreakAST>();
    } else {
        ret = parse_expr_start();
    }
    if (cur_tok != tok_newline && cur_tok != tok_eof) {
        lexer.log_unexpected();
        while (cur_tok != tok_newline && cur_tok != tok_eof)
            advance();
    }
    advance();
    return {std::move(ret), -1};
}

ASTPtr Parser::parse_expr_start() {
    ASTPtr left;
    if (cur_tok == tok_identifier) {
        auto name_cap = make_capture(lexer.get_word(), lexer);
        advance();
        if (cur_tok == tok_lpar) {
            left = parse_func_call(name_cap.value, name_cap.loc);
        } else {
            auto lvalue = parse_lvalue(name_cap.value, name_cap.loc, false);
            if (cur_tok == tok_colon || (tok_assn <= cur_tok && cur_tok <= tok_xor_assn)) {
                return parse_local_var(std::move(lvalue), std::move(name_cap));
            } else if (!zulctx.var_exist(name_cap.value)) {
                System::logger.log_error(name_cap.loc, name_cap.word_size,
                                         {"\"", name_cap.value, "\" 는 존재하지 않는 변수입니다"});
                return nullptr;
            }
            left = std::move(lvalue);
        }
    } else {
        left = parse_primary();
    }
    if (!left)
        return nullptr;
    if (get_op_prec() == op_prec_map[tok_assn]) {
        lexer.log_token("대입 연산을 사용할 수 없습니다. 식의 좌변이 적절한 좌측값이 아닙니다");
        while (cur_tok != tok_newline && cur_tok != tok_eof)
            advance();
        return left;
    }
    return parse_bin_op(0, std::move(left));
}

ASTPtr Parser::parse_local_var(std::unique_ptr<LvalueAST> lvalue, Capture<std::string> name_cap) {
    bool is_exist = zulctx.var_exist(name_cap.value);
    auto op_cap = make_capture(cur_tok, lexer);
    if (op_cap.value == tok_colon) { //선언
        if (is_exist) {
            System::logger.log_error(name_cap.loc, name_cap.word_size, "변수가 재정의되었습니다");
            return nullptr;
        }
        advance();
        auto type = parse_type(true);
        if (cur_tok == tok_assn) { //선언 + 초기화
            advance();
            ASTPtr body = parse_expr();
            if (!body)
                return nullptr;
            return make_unique<VariableDeclAST>(std::move(name_cap), zulctx, type.first, std::move(body));
        }
        return make_unique<VariableDeclAST>(std::move(name_cap), zulctx, type.first);
    }
    //자동추론 + 초기화
    advance();
    ASTPtr body = parse_expr();
    if (!body) {
        return nullptr;
    }
    if (!is_exist) {
        if (op_cap.value == tok_assn) {
            return make_unique<VariableDeclAST>(std::move(name_cap), zulctx, std::move(body));
        } else {
            System::logger.log_error(name_cap.loc, name_cap.word_size, {"\"", name_cap.value, "\" 는 존재하지 않는 변수입니다"});
            return nullptr;
        }
    }
    return make_unique<VariableAssnAST>(std::move(lvalue), std::move(op_cap), std::move(body));
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
            lexer.log_token("하나의 식에는 하나의 대입 연산자만 사용할 수 있습니다");
            while (cur_tok != tok_newline && cur_tok != tok_eof)
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
    switch (cur_tok) {
        case tok_identifier:
            return parse_identifier();
        case tok_real:
        case tok_int:
            return parse_num();
        case tok_true:
            advance();
            return make_unique<ImmBoolAST>(true);
        case tok_false:
            advance();
            return make_unique<ImmBoolAST>(false);
        case tok_lpar:
            return parse_par();
        case tok_dquotes:
            return parse_str();
        case tok_squotes:
            return parse_char();
        case tok_add:
        case tok_sub:
        case tok_not:
        case tok_bitnot:
            return parse_unary_op();
        default:
            return nullptr;
    }
}

pair<ASTPtr, bool> Parser::parse_if_header() {
    ASTPtr cond;
    bool error = false;
    cond = parse_expr();
    if (!cond) {
        lexer.log_unexpected("조건식이 필요합니다");
        error = true;
    }
    if (cur_tok != tok_colon) {
        lexer.log_unexpected("콜론이 필요합니다");
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
    advance(); //ㅇㅈ? 지나치기
    auto [if_cond, error] = parse_if_header();
    auto [if_body, stop_level] = parse_block_body(target_level);
    zulctx.remove_top_scope_vars();
    if (if_body.empty() && !System::logger.has_error()) {
        lexer.log_token("ㅇㅈ?문의 몸체가 정의되지 않았습니다");
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
            lexer.log_token("ㄴㄴ?문의 몸체가 정의되지 않았습니다");
            error = true;
        }
        elif_pair_list.emplace_back(std::move(elif_cond), std::move(elif_body));
    }
//---------------------------------else문 파싱---------------------------------
    if (stop_level == target_level - 1 && cur_tok == tok_nope) {
        advance();
        zulctx.scope_stack.emplace();
        if (cur_tok != tok_colon) {
            lexer.log_unexpected("콜론이 필요합니다");
            error = true;
        }
        advance();
        auto [body, level] = parse_block_body(target_level);
        zulctx.remove_top_scope_vars();
        stop_level = level;
        if (body.empty() && !System::logger.has_error()) {
            lexer.log_token("ㄴㄴ문의 몸체가 정의되지 않았습니다");
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
    advance(); //ㄱㄱ 지나치기
    auto expr = parse_expr_start();
    if (cur_tok == tok_semicolon) {
        init_for = std::move(expr);
        advance();
        test_for = parse_expr_start();
        if (cur_tok != tok_semicolon) {
            lexer.log_unexpected("ㄱㄱ문에는 세미콜론이 아예 오지 않거나, 2개가 와야 합니다. 세미콜론이 필요합니다");
        }
        advance();
        update_for = parse_expr_start();
    } else if (expr) {
        test_for = std::move(expr);
    }
    if (cur_tok != tok_colon) {
        lexer.log_unexpected("콜론이 와야 합니다");
    }
    advance();
//---------------------------------for문 몸체 파싱---------------------------------
    bool in_loop = zulctx.in_loop;
    zulctx.in_loop = true;
    auto [for_body, stop_level] = parse_block_body(target_level);
    zulctx.in_loop = in_loop;
    zulctx.remove_top_scope_vars();
    if (for_body.empty() && !System::logger.has_error()) {
        lexer.log_token("ㄱㄱ문의 몸체가 정의되지 않았습니다");
        return {nullptr, stop_level};
    }
    return {make_unique<LoopAST>(std::move(init_for), std::move(test_for), std::move(update_for),
                                 std::move(for_body)), stop_level};
}

ASTPtr Parser::parse_identifier() {
    auto name = lexer.get_word();
    auto loc = lexer.get_token_loc();
    advance();
    if (cur_tok == tok_lpar)
        return parse_func_call(name, loc);
    return parse_lvalue(name, loc);
}

ASTPtr Parser::parse_func_call(string &name, pair<int, int> name_loc) {
    if (!func_proto_map.contains(name)) {
        System::logger.log_error(name_loc, name.size(), {"\"", name, "\" 는 존재하지 않는 함수입니다"});
        return nullptr;
    }
    advance(); //(
    vector<Capture<ASTPtr>> args;
    while (true) {
        if (cur_tok == tok_rpar) {
            auto &proto = func_proto_map[name];
            auto param_cnt = proto.params.size();
            if (!proto.is_var_arg && param_cnt != args.size()) {
                lexer.log_token({"인자 개수가 맞지 않습니다. ", "\"", name, "\" 함수의 인자 개수는 ", to_string(param_cnt), "개 입니다."});
                advance();
                return nullptr;
            }
            advance(); // )
            return make_unique<FuncCallAST>(proto, std::move(args));
        }
        auto arg_start_loc = lexer.get_token_loc();
        auto arg = parse_expr();
        if (!arg)
            return nullptr;
        args.emplace_back(std::move(arg), arg_start_loc, lexer.get_token_loc().second - arg_start_loc.second);
        if (cur_tok == tok_comma) {
            advance();
        } else if (cur_tok == tok_eof) {
            lexer.log_unexpected("괄호가 닫히지 않았습니다. )가 필요합니다");
        } else if (cur_tok != tok_rpar) {
            lexer.log_unexpected("콤마가 필요합니다");
        }
    }
}

unique_ptr<LvalueAST> Parser::parse_lvalue(string &name, pair<int, int> name_loc, bool check_exist) {
    if (check_exist && !zulctx.var_exist(name)) {
        System::logger.log_error(name_loc, name.size(), {"\"", name, "\" 는 존재하지 않는 변수입니다"});
        if (cur_tok == tok_lsqbrk)
            parse_subscript();
        return nullptr;
    }
    if (cur_tok == tok_lsqbrk) {
        auto loc = lexer.get_token_loc();
        auto size = lexer.get_word().size();
        auto index = parse_subscript();
        if (!index)
            return nullptr;
        return make_unique<SubscriptAST>(make_unique<VariableAST>(name),
                                         Capture<ASTPtr>(std::move(index), loc, size));
    }
    return make_unique<VariableAST>(name);
}

ASTPtr Parser::parse_subscript() {
    advance(); // [
    auto ret = parse_expr();
    if (cur_tok != tok_rsqbrk) {
        lexer.log_unexpected("대괄호가 닫히지 않았습니다. ]가 필요합니다");
        advance();
        return nullptr;
    }
    if (!ret)
        lexer.log_token("잘못된 연산자 사용입니다. [] 안에 표현식이 필요합니다.");
    advance(); // ]
    return ret;
}

pair<int, ASTPtr> Parser::parse_type(bool no_arr) {
    pair<int, ASTPtr> null{-1, nullptr};
    if (cur_tok != tok_identifier) {
        lexer.log_unexpected("타입 이름이 와야 합니다");
        advance();
        return null;
    }
    auto type_name = lexer.get_word();
    if (!type_map.contains(type_name)) {
        lexer.log_unexpected("존재하지 않는 타입입니다");
        advance();
        return null;
    }
    int type_id = type_map[type_name];
    advance();
    if (cur_tok != tok_lsqbrk) {
        return {type_id, nullptr};
    }
    if (no_arr) {
        lexer.log_token("배열 타입은 전역 변수만 가능합니다");
        while (cur_tok == tok_lsqbrk)
            parse_subscript(); //[]먹기
        return {type_id, nullptr};
    }
    auto brk_loc = lexer.get_token_loc();
    auto brk_body = parse_subscript();
    if (!brk_body) {
        System::logger.log_error(brk_loc, 1, "배열 크기를 명시해야 합니다");
        return {type_id, nullptr};
    }
    if (cur_tok == tok_lsqbrk) {
        lexer.log_token("다차원 배열은 지원되지 않습니다");
        while (cur_tok == tok_lsqbrk)
            parse_subscript(); //[]먹기
    }
    return {type_id + TYPE_COUNTS, std::move(brk_body)};
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
        lexer.log_unexpected("괄호가 닫히지 않았습니다. )가 필요합니다");
        advance();
        return nullptr;
    }
    advance(); // )
    return ret;
}

ASTPtr Parser::parse_num() {
    auto num_word = lexer.get_word();
    char *end_ptr;
    Guard g{[this]() { this->advance(); }};

    if (cur_tok == tok_int) {
        auto result = strtoll(num_word.c_str(), &end_ptr, 10);
        if (errno != 0) {
            lexer.log_token("잘못된 수 리터럴입니다. 오버플로우가 발생했습니다");
            return nullptr;
        }
        return make_unique<ImmIntAST>(result);
    } else {
        auto result = strtod(num_word.c_str(), &end_ptr);
        if (errno != 0) {
            lexer.log_token("잘못된 실수 리터럴입니다");
            return nullptr;
        }
        return make_unique<ImmRealAST>(result);
    }
}

ASTPtr Parser::parse_str() {
    int st = lexer.get_line_index() + 1;
    do {
        advance();
        if (cur_tok == tok_newline || cur_tok == tok_eof) {
            lexer.log_unexpected("쌍따옴표가 닫히지 않았습니다. \"가 필요합니다");
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
            lexer.log_unexpected("따옴표가 닫히지 않았습니다. \'가 필요합니다");
            return nullptr;
        }
    } while (cur_tok != tok_squotes);
    int ed = lexer.get_line_index();
    auto str = lexer.get_line_substr(st, ed);
    if (str.size() > 1) {
        lexer.log_token("\"글자\" 자료형은 1바이트입니다. 자료형의 범위를 초과합니다");
        advance();
        return nullptr;
    } else if (str.empty()) {
        lexer.log_token("빈 글자는 존재할 수 없습니다");
        advance();
        return nullptr;
    }
    advance();
    return make_unique<ImmCharAST>(str[0]);
}

void Parser::create_func(FuncProtoAST &proto, const vector<ASTPtr> &body, std::pair<int, int> name_loc, bool exist) {
    llvm::Function *llvm_func;
    if (exist) {
        llvm_func = zulctx.module->getFunction(proto.name);
    } else {
        llvm_func = proto.code_gen(zulctx);
    }
    auto entry_block = BasicBlock::Create(*zulctx.context, "entry", llvm_func);
    zulctx.builder.SetInsertPoint(entry_block);
    llvm::IRBuilder<> entry_builder(entry_block, entry_block->begin());

    if (zulctx.ret_count > 1) {
        if (proto.return_type != -1)
            zulctx.return_var = entry_builder.CreateAlloca(get_llvm_type(*zulctx.context, proto.return_type), nullptr,
                                                           "ret");
        zulctx.return_block = BasicBlock::Create(*zulctx.context, "func_ret");
    }

    int i = 0;
    for (auto &arg: llvm_func->args()) {
        auto alloca_val = entry_builder.CreateAlloca(arg.getType(), nullptr, proto.params[i].first);
        zulctx.builder.CreateStore(&arg, alloca_val);
        zulctx.local_var_map[proto.params[i].first] = make_pair(alloca_val, proto.params[i].second);
        i++;
    }

    for (auto &ast: body) {
        auto code = ast->code_gen(zulctx).second;
        if (code == id_interrupt)
            break;
    }

    auto cur_block = zulctx.builder.GetInsertBlock();
    if (zulctx.ret_count == 0 || cur_block->empty() ||
        (!isa<BranchInst>(cur_block->back()) && !isa<ReturnInst>(cur_block->back()))) {
        if (proto.name == ENTRY_FN_NAME) {
            if (zulctx.ret_count > 1)
                zulctx.builder.CreateBr(zulctx.return_block);
            else
                zulctx.builder.CreateRet(ConstantInt::get(Type::getInt64Ty(*zulctx.context), 0, true));
        } else if (proto.return_type == -1) {
            zulctx.builder.CreateRetVoid();
        } else if (!remove_all_pred(cur_block)) {
            System::logger.log_error(name_loc, proto.name.size(),
                                     {"\"", proto.name, "\" 함수의 리턴 타입은 \"", get_type_name(proto.return_type),
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
