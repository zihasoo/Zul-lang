
//SPDX-FileCopyrightText: © 2023 ByungYun Lee
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "AST.h"

using std::string;
using std::string_view;
using std::unique_ptr;
using std::pair;
using std::vector;
using std::unordered_map;
using std::make_unique;
using std::make_pair;
using std::to_string;
using std::max;

using llvm::LLVMContext;
using llvm::ConstantInt;
using llvm::ConstantFP;
using llvm::ConstantPointerNull;
using llvm::Type;
using llvm::PointerType;
using llvm::Value;
using llvm::Constant;

bool ExprAST::is_const() {
    return false;
}

bool ExprAST::is_lvalue() {
    return false;
}

int ExprAST::get_typeid(ZulContext &zulctx) {
    return -1;
}

bool LvalueAST::is_lvalue() {
    return true;
}

FuncProtoAST::FuncProtoAST(string name, int return_type, vector<pair<string, int>> params, bool has_body,
                           bool is_var_arg) :
        name(std::move(name)), return_type(return_type), params(std::move(params)), has_body(has_body),
        is_var_arg(is_var_arg) {}

llvm::Function *FuncProtoAST::code_gen(ZulContext &zulctx) {
    vector<llvm::Type *> param_types;
    param_types.reserve(params.size());
    for (auto &param: params) {
        param_types.emplace_back(get_llvm_type(*zulctx.context, param.second));
    }
    auto func_type = llvm::FunctionType::get(get_llvm_type(*zulctx.context, return_type), param_types, is_var_arg);
    return llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, *zulctx.module);
}

FuncRetAST::FuncRetAST(ASTPtr body, Capture<int> return_type) :
        body(std::move(body)), return_type(std::move(return_type)) {}

ZulValue FuncRetAST::code_gen(ZulContext &zulctx) {
    ZulValue body_value = nullzul;
    if (body) {
        body_value = body->code_gen(zulctx);
        if (!body_value.first)
            return nullzul;
    }
    if (return_type.value == -1) {
        if (zulctx.ret_count == 1) {
            zulctx.builder.CreateRetVoid();
        } else {
            zulctx.builder.CreateBr(zulctx.return_block);
        }
    } else {
        if (return_type.value != body_value.second && !create_cast(zulctx, body_value, return_type.value)) {
            System::logger.log_error(return_type.loc, return_type.word_size,
                                     {"리턴 타입이 일치하지 않습니다. 반환 구문의 타입 \"", get_type_name(body_value.second),
                                      "\" 에서 리턴 타입 \"", get_type_name(return_type.value), "\" 로 캐스팅 할 수 없습니다"});
        } else if (zulctx.ret_count == 1) {
            zulctx.builder.CreateRet(body_value.first);
        } else {
            zulctx.builder.CreateStore(body_value.first, zulctx.return_var);
            zulctx.builder.CreateBr(zulctx.return_block);
        }
    }
    return {nullptr, id_interrupt};
}

int FuncRetAST::get_typeid(ZulContext &zulctx) {
    return return_type.value;
}

IfAST::IfAST(CondBodyPair if_pair, std::vector<CondBodyPair> elif_pair_list, std::vector<ASTPtr> else_body) :
        if_pair(std::move(if_pair)), elif_pair_list(std::move(elif_pair_list)), else_body(std::move(else_body)) {}

ZulValue IfAST::code_gen(ZulContext &zulctx) {
    auto prev_cond = if_pair.first->code_gen(zulctx);
    if (!prev_cond.first || !to_boolean_expr(zulctx, prev_cond))
        return nullzul;

    auto func = zulctx.builder.GetInsertBlock()->getParent();
    auto body_block = llvm::BasicBlock::Create(*zulctx.context, "if", func);
    auto merge_block = llvm::BasicBlock::Create(*zulctx.context, "merge");
    auto prev_block = zulctx.builder.GetInsertBlock();

    zulctx.builder.SetInsertPoint(body_block);
    bool interrupted = false;
    for (auto &ast: if_pair.second) {
        auto code = ast->code_gen(zulctx).second;
        if (code == id_interrupt) {
            interrupted = true;
            break;
        }
    }
    if (!interrupted)
        zulctx.builder.CreateBr(merge_block);

    for (auto &elif_pair: elif_pair_list) {
        auto elif_cond_block = llvm::BasicBlock::Create(*zulctx.context, "elif_cond", func);
        zulctx.builder.SetInsertPoint(prev_block);
        zulctx.builder.CreateCondBr(prev_cond.first, body_block, elif_cond_block);

        zulctx.builder.SetInsertPoint(elif_cond_block);
        prev_cond = elif_pair.first->code_gen(zulctx);
        if (!prev_cond.first || !to_boolean_expr(zulctx, prev_cond))
            return nullzul;

        body_block = llvm::BasicBlock::Create(*zulctx.context, "elif", func);
        zulctx.builder.SetInsertPoint(body_block);
        interrupted = false;
        for (auto &ast: elif_pair.second) {
            auto code = ast->code_gen(zulctx).second;
            if (code == id_interrupt) {
                interrupted = true;
                break;
            }
        }
        if (!interrupted)
            zulctx.builder.CreateBr(merge_block);
        prev_block = elif_cond_block;
    }
    if (else_body.empty()) {
        zulctx.builder.SetInsertPoint(prev_block);
        zulctx.builder.CreateCondBr(prev_cond.first, body_block, merge_block);
    } else {
        auto else_block = llvm::BasicBlock::Create(*zulctx.context, "else", func);
        zulctx.builder.SetInsertPoint(prev_block);
        zulctx.builder.CreateCondBr(prev_cond.first, body_block, else_block);

        zulctx.builder.SetInsertPoint(else_block);
        interrupted = false;
        for (auto &ast: else_body) {
            auto code = ast->code_gen(zulctx).second;
            if (code == id_interrupt) {
                interrupted = true;
                break;
            }
        }
        if (!interrupted)
            zulctx.builder.CreateBr(merge_block);
    }

    func->insert(func->end(), merge_block);
    zulctx.builder.SetInsertPoint(merge_block);
    return nullzul;
}

LoopAST::LoopAST(ASTPtr init_body, ASTPtr test_body, ASTPtr update_body, std::vector<ASTPtr> loop_body) :
        init_body(std::move(init_body)), test_body(std::move(test_body)), update_body(std::move(update_body)),
        loop_body(std::move(loop_body)) {}

ZulValue LoopAST::code_gen(ZulContext &zulctx) {
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    auto test_block = llvm::BasicBlock::Create(*zulctx.context, "loop_test", func);
    auto start_block = llvm::BasicBlock::Create(*zulctx.context, "loop_start", func);
    auto update_block = llvm::BasicBlock::Create(*zulctx.context, "loop_update", func);
    auto end_block = llvm::BasicBlock::Create(*zulctx.context, "loop_end", func);
    zulctx.loop_update_stack.push(update_block);
    zulctx.loop_end_stack.push(end_block);
    Guard guard{[&zulctx]() { //리턴 할 때 자동으로 pop하도록 가드 생성
        zulctx.loop_update_stack.pop();
        zulctx.loop_end_stack.pop();
    }};

    if (init_body && !init_body->code_gen(zulctx).first)
        return nullzul;

    zulctx.builder.CreateBr(test_block);
    zulctx.builder.SetInsertPoint(test_block);
    if (test_body) {
        ZulValue test_cond = test_body->code_gen(zulctx);
        if (!test_cond.first || !to_boolean_expr(zulctx, test_cond))
            return nullzul;
        zulctx.builder.CreateCondBr(test_cond.first, start_block, end_block);
    } else {
        zulctx.builder.CreateBr(start_block);
    }

    bool interrupted = false;
    zulctx.builder.SetInsertPoint(start_block);
    for (auto &ast: loop_body) {
        auto code = ast->code_gen(zulctx);
        if (code.second == id_interrupt) {
            interrupted = true;
            break;
        }
    }
    if (!interrupted)
        zulctx.builder.CreateBr(update_block);

    zulctx.builder.SetInsertPoint(update_block);
    if (update_body && !update_body->code_gen(zulctx).first)
        return nullzul;
    zulctx.builder.CreateBr(test_block);

    zulctx.builder.SetInsertPoint(end_block);
    return nullzul;
}

ZulValue ContinueAST::code_gen(ZulContext &zulctx) {
    zulctx.builder.CreateBr(zulctx.loop_update_stack.top());
    return {nullptr, id_interrupt};
}

ZulValue BreakAST::code_gen(ZulContext &zulctx) {
    zulctx.builder.CreateBr(zulctx.loop_end_stack.top());
    return {nullptr, id_interrupt};
}

VariableAST::VariableAST(string name) : name(std::move(name)) {}

ZulValue VariableAST::get_origin_value(ZulContext &zulctx) {
    if (zulctx.local_var_map.contains(name))
        return zulctx.local_var_map[name];
    if (zulctx.global_var_map.contains(name))
        return zulctx.global_var_map[name];
    return nullzul;
}

ZulValue VariableAST::code_gen(ZulContext &zulctx) {
    auto value = get_origin_value(zulctx);
    if (!value.first)
        return nullzul;
    if (value.second < TYPE_COUNTS || value.second >= TYPE_COUNTS * 2)
        value.first = zulctx.builder.CreateLoad(get_llvm_type(*zulctx.context, value.second), value.first);
    return value;
}

int VariableAST::get_typeid(ZulContext &zulctx) {
    return get_origin_value(zulctx).second;
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, ZulContext &zulctx, int type, ASTPtr body) :
        name(std::move(name)), type(type), body(std::move(body)) {
    register_var(zulctx);
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, ZulContext &zulctx, ASTPtr body) :
        name(std::move(name)), type(-1), body(std::move(body)) {
    register_var(zulctx);
}

void VariableDeclAST::register_var(ZulContext &zulctx) {
    int t = (type == -1 ? body->get_typeid(zulctx) : type);
    zulctx.local_var_map.emplace(name.value, make_pair(nullptr, t)); //이름만 등록 해놓기
    if (!zulctx.scope_stack.empty()) { //만약 스코프 안에 있다면
        zulctx.scope_stack.top().push_back(name.value); //가장 가까운 스코프에 변수 등록
    }
}

ZulValue VariableDeclAST::code_gen(ZulContext &zulctx) {
    Value *init_val = nullptr;
    if (body) { //대입식이 있으면 식 먼저 생성
        auto result = body->code_gen(zulctx);
        if (!result.first)
            return nullzul;
        if (type != -1 && type != result.second && !create_cast(zulctx, result, type)) {
            System::logger.log_error(name.loc, name.word_size,
                                     {"대입 연산식의 타입 \"",
                                      get_type_name(result.second), "\" 에서 변수의 타입 \"",
                                      get_type_name(type),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
        if (type == -1) //타입 명시가 안됐을 때만
            type = result.second; //추론된 타입 적용
        init_val = result.first;
    }
    if (type == -1) {
        System::logger.log_error(name.loc, name.word_size, {"\"", get_type_name(type), "\" 타입의 변수를 생성할 수 없습니다"});
        return nullzul;
    }
    if (TYPE_COUNTS < type && type < TYPE_COUNTS * 2) //배열 타입일 경우 포인터로 변환함
        type += TYPE_COUNTS;
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    llvm::IRBuilder<> entry_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    auto alloca_val = entry_builder.CreateAlloca(get_llvm_type(*zulctx.context, type), nullptr, name.value);
    zulctx.local_var_map[name.value] = make_pair(alloca_val, type);

    if (body)
        zulctx.builder.CreateStore(init_val, alloca_val);
    return {alloca_val, type};
}

VariableAssnAST::VariableAssnAST(unique_ptr<LvalueAST> target, Capture<Token> op, ASTPtr body) :
        target(std::move(target)), op(std::move(op)), body(std::move(body)) {}

ZulValue VariableAssnAST::code_gen(ZulContext &zulctx) {
    static unordered_map<Token, Token> assn_op_map = {
            {tok_mul_assn,    tok_mul},
            {tok_div_assn,    tok_div},
            {tok_mod_assn,    tok_mod},
            {tok_add_assn,    tok_add},
            {tok_sub_assn,    tok_sub},
            {tok_lshift_assn, tok_lshift},
            {tok_rshift_assn, tok_rshift},
            {tok_and_assn,    tok_bitand},
            {tok_or_assn,     tok_bitor},
            {tok_xor_assn,    tok_bitxor}
    };
    auto target_value = target->code_gen(zulctx);
    auto body_value = body->code_gen(zulctx);

    if (!target_value.first || !body_value.first ||
        target_value.second > id_float || body_value.second > id_float) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  get_type_name(target_value.second), "\" 와 변수의 타입 \"",
                                  get_type_name(body_value.second),
                                  "\" 는 연산이 불가능합니다"});
        return nullzul;
    }

    if (target_value.second != body_value.second && !create_cast(zulctx, body_value, target_value.second)) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  get_type_name(target_value.second), "\" 에서 변수의 타입 \"",
                                  get_type_name(body_value.second),
                                  "\" 로 캐스팅 할 수 없습니다"});
        return nullzul;
    }
    llvm::Value *result;
    if (op.value == tok_assn) {
        result = body_value.first;
    } else {
        auto prac_op = Capture(assn_op_map[op.value], op.loc, op.word_size);
        if (target_value.second < id_float) {
            result = create_int_operation(zulctx, target_value.first, body_value.first, prac_op);
        } else {
            result = create_float_operation(zulctx, target_value.first, body_value.first, prac_op);
        }
        if (!result)
            return nullzul;
    }
    return {zulctx.builder.CreateStore(result, target->get_origin_value(zulctx).first), target_value.second};
}

BinOpAST::BinOpAST(ASTPtr left, ASTPtr right, Capture<Token> op) : left(std::move(left)),
                                                                   right(std::move(right)),
                                                                   op(std::move(op)) {}

ZulValue BinOpAST::short_circuit_code_gen(ZulContext &zulctx) const {
    auto lhs = left->code_gen(zulctx);
    if (!lhs.first)
        return nullzul;
    if (!to_boolean_expr(zulctx, lhs)) {
        System::logger.log_error(op.loc, op.word_size, "좌측항을 \"논리\" 자료형으로 캐스팅 할 수 없습니다");
        return nullzul;
    }

    auto origin_block = zulctx.builder.GetInsertBlock();
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    auto sc_test = llvm::BasicBlock::Create(*zulctx.context, "sc_test", func);
    auto sc_end = llvm::BasicBlock::Create(*zulctx.context, "sc_end", func);

    if (op.value == tok_and)
        zulctx.builder.CreateCondBr(lhs.first, sc_test, sc_end);
    else
        zulctx.builder.CreateCondBr(lhs.first, sc_end, sc_test);

    zulctx.builder.SetInsertPoint(sc_test);
    auto rhs = right->code_gen(zulctx);
    if (!rhs.first)
        return nullzul;
    if (!to_boolean_expr(zulctx, rhs)) {
        System::logger.log_error(op.loc, op.word_size, "우측항을 \"논리\" 자료형으로 캐스팅 할 수 없습니다");
        return nullzul;
    }
    zulctx.builder.CreateBr(sc_end);

    zulctx.builder.SetInsertPoint(sc_end);
    auto phi = zulctx.builder.CreatePHI(get_llvm_type(*zulctx.context, 0), 2);
    phi->addIncoming(llvm::ConstantInt::getBool(*zulctx.context, op.value == tok_or), origin_block);
    phi->addIncoming(rhs.first, sc_test);
    return {phi, 0};
}

ZulValue BinOpAST::code_gen(ZulContext &zulctx) {
    if (op.value == tok_and || op.value == tok_or) {
        return short_circuit_code_gen(zulctx);
    }
    auto lhs = left->code_gen(zulctx);
    auto rhs = right->code_gen(zulctx);
    if (!lhs.first || !rhs.first)
        return nullzul;
    if (lhs.second > id_float || rhs.second > id_float) { //연산자 오버로딩 지원 하게되면 변경
        System::logger.log_error(op.loc, op.word_size, {"좌측항의 타입 \"",
                                                        get_type_name(lhs.second), "\" 와 우측항의 타입 \"",
                                                        get_type_name(rhs.second),
                                                        "\" 는 연산이 불가능합니다"});
        return nullzul;
    }
    int calc_type = lhs.second;
    if (lhs.second > rhs.second) {
        calc_type = lhs.second;
        if (!create_cast(zulctx, rhs, lhs.second)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"우측항의 타입 \"",
                                      get_type_name(rhs.second), "\" 에서 좌측항의 타입 \"",
                                      get_type_name(lhs.second),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    } else if (lhs.second < rhs.second) {
        calc_type = rhs.second;
        if (!create_cast(zulctx, lhs, rhs.second)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"좌측항의 타입 \"",
                                      get_type_name(lhs.second), "\" 에서 우측항의 타입 \"",
                                      get_type_name(rhs.second),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    }
    llvm::Value *ret;
    if (calc_type < id_float) {
        ret = create_int_operation(zulctx, lhs.first, rhs.first, op);
    } else {
        ret = create_float_operation(zulctx, lhs.first, rhs.first, op);
    }
    calc_type = is_cmp(op.value) ? id_bool : calc_type;
    return {ret, calc_type};
}

bool BinOpAST::is_const() {
    return left->is_const() && right->is_const() && op.value != tok_and && op.value != tok_or;
}

int BinOpAST::get_typeid(ZulContext &zulctx) {
    if (op.value == tok_and || op.value == tok_or)
        return 0;
    int ltype = left->get_typeid(zulctx);
    int rtype = right->get_typeid(zulctx);
    if (ltype > id_float || rtype > id_float || ltype < id_bool || rtype < id_bool)
        return -1;
    return max(ltype, rtype);
}

UnaryOpAST::UnaryOpAST(ASTPtr body, Capture<Token> op) : body(std::move(body)), op(std::move(op)) {}

ZulValue UnaryOpAST::code_gen(ZulContext &zulctx) {
    auto body_value = body->code_gen(zulctx);
    auto zero = get_const_zero(body_value.first->getType(), body_value.second);
    if (!body_value.first)
        return nullzul;
    if (body_value.second > id_float) {
        System::logger.log_error(op.loc, op.word_size, "단항 연산자를 적용할 수 없습니다");
        return nullzul;
    }
    switch (op.value) {
        case tok_add:
            break;
        case tok_sub:
            if (body_value.second < id_float)
                return {zulctx.builder.CreateSub(zero, body_value.first), body_value.second};
            else
                return {zulctx.builder.CreateFSub(zero, body_value.first), body_value.second};
        case tok_not:
            if (body_value.second < id_float)
                return {zulctx.builder.CreateICmpEQ(zero, body_value.first), 0};
            else
                return {zulctx.builder.CreateFCmpOEQ(zero, body_value.first), 0};
        case tok_bitnot:
            if (body_value.second == id_float) {
                System::logger.log_error(op.loc, op.word_size, "단항 '~' 연산자를 적용할 수 없습니다");
                return nullzul;
            }
            return {zulctx.builder.CreateNot(body_value.first), body_value.second};
        default:
            System::logger.log_error(op.loc, op.word_size, "올바른 단항 연산자가 아닙니다");
            return nullzul;
    }
    return body_value;
}

bool UnaryOpAST::is_const() {
    return body->is_const();
}

int UnaryOpAST::get_typeid(ZulContext &zulctx) {
    if (op.value == tok_not)
        return 0;
    return body->get_typeid(zulctx);
}

SubscriptAST::SubscriptAST(std::unique_ptr<VariableAST> target, Capture<ASTPtr> index) : target(std::move(target)),
                                                                                         index(std::move(index)) {}

ZulValue SubscriptAST::get_origin_value(ZulContext &zulctx) {
    auto target_val = target->get_origin_value(zulctx);
    auto index_val = index.value->code_gen(zulctx);
    if (!target_val.first || !index_val.first)
        return nullzul;
    if (target_val.second < TYPE_COUNTS) {
        System::logger.log_error(index.loc, index.word_size, "'[]' 연산자를 사용할 수 없습니다. 배열이 아닙니다.");
        return nullzul;
    }
    if (index_val.second != 2) {
        System::logger.log_error(index.loc, index.word_size, "배열의 인덱스는 정수여야 합니다");
        return nullzul;
    }
    auto elm_type = get_llvm_type(*zulctx.context, target_val.second - TYPE_COUNTS);
    auto elm_ptr = zulctx.builder.CreateGEP(elm_type, target_val.first, {index_val.first});
    return {elm_ptr, target_val.second - TYPE_COUNTS};
}

ZulValue SubscriptAST::code_gen(ZulContext &zulctx) {
    auto elm_ptr = get_origin_value(zulctx);
    auto loaded = zulctx.builder.CreateLoad(get_llvm_type(*zulctx.context, elm_ptr.second), elm_ptr.first);
    return {loaded, elm_ptr.second};
}

int SubscriptAST::get_typeid(ZulContext &zulctx) {
    return target->get_typeid(zulctx) - TYPE_COUNTS;
}

FuncCallAST::FuncCallAST(FuncProtoAST &proto, vector<Capture<ASTPtr>> args)
        : proto(proto), args(std::move(args)) {}


std::string_view FuncCallAST::get_format_str(int type_id) {
    if (format_str_map.contains(type_id))
        return format_str_map[type_id];
    return "%p";
}

ZulValue FuncCallAST::handle_std_in(ZulContext &zulctx) {
    vector<llvm::Value *> arg_values;
    string format_str;
    auto s = args.size();
    bool has_error = false;
    arg_values.reserve(args.size());
    format_str.reserve(s * 3);
    for (int i = 0; i < s; i++) {
        if (!args[i].value->is_lvalue()) {
            System::logger.log_error(args[i].loc, args[i].word_size, {"\"", STDIN_NAME, "\" 함수에는 좌측값만 올 수 있습니다"});
            has_error = true;
        }
        auto arg = static_cast<LvalueAST *>(args[i].value.get())->get_origin_value(zulctx);
        if (!arg.first)
            return nullzul;

        format_str.append(get_format_str(arg.second));
        if (i < s - 1)
            format_str.push_back(' ');
        arg_values.push_back(arg.first);
    }
    arg_values.insert(arg_values.begin(), zulctx.builder.CreateGlobalStringPtr(format_str));
    if (has_error)
        return nullzul;
    return {zulctx.builder.CreateCall(zulctx.module->getFunction("scanf"), arg_values), -1};
}

ZulValue FuncCallAST::handle_std_out(ZulContext &zulctx) {
    vector<llvm::Value *> arg_values;
    string format_str;
    auto s = args.size();
    arg_values.reserve(args.size());
    format_str.reserve(s * 3);
    for (int i = 0; i < s; i++) {
        auto arg = args[i].value->code_gen(zulctx);
        if (!arg.first)
            return nullzul;
        format_str.append(get_format_str(arg.second));
        format_str.push_back((i == s - 1) ? '\n' : ' ');
        arg_values.push_back(arg.first);
    }
    arg_values.insert(arg_values.begin(), zulctx.builder.CreateGlobalStringPtr(format_str));
    return {zulctx.builder.CreateCall(zulctx.module->getFunction("printf"), arg_values), -1};
}

ZulValue FuncCallAST::code_gen(ZulContext &zulctx) {
    if (proto.name == STDIN_NAME)
        return handle_std_in(zulctx);
    if (proto.name == STDOUT_NAME)
        return handle_std_out(zulctx);
    auto target_func = zulctx.module->getFunction(proto.name);
    vector<llvm::Value *> arg_values;
    bool has_error = false;
    arg_values.reserve(args.size());
    for (int i = 0; i < args.size(); i++) {
        auto arg = args[i].value->code_gen(zulctx);
        if (!arg.first)
            return nullzul;
        if (i < proto.params.size() && arg.second != proto.params[i].second &&
            !create_cast(zulctx, arg, proto.params[i].second)) {
            //arg와 param의 타입이 맞지 않으면 캐스팅 시도
            System::logger.log_error(args[i].loc, args[i].word_size, {
                    "인자의 타입 \"", get_type_name(arg.second), "\" 에서 매개변수의 타입 \"",
                    get_type_name(proto.params[i].second), "\" 로 캐스팅 할 수 없습니다"});
            has_error = true;
        }
        arg_values.push_back(arg.first);
    }
    if (has_error)
        return nullzul;
    return {zulctx.builder.CreateCall(target_func, arg_values), proto.return_type};
}

int FuncCallAST::get_typeid(ZulContext &zulctx) {
    return proto.return_type;
}

unordered_map<int, string_view> FuncCallAST::format_str_map = {
        {id_bool,                   "%u"},
        {id_char,                   "%c"},
        {id_int,                    "%lld"},
        {id_float,                  "%lf"},
        {id_char + TYPE_COUNTS,     "%s"},
        {id_char + TYPE_COUNTS * 2, "%s"},
};

ImmBoolAST::ImmBoolAST(bool val) : val(val) {}

ZulValue ImmBoolAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(1, val)), id_bool};
}

bool ImmBoolAST::is_const() {
    return true;
}

int ImmBoolAST::get_typeid(ZulContext &zulctx) {
    return id_bool;
}

ImmCharAST::ImmCharAST(char val) : val(val) {}

ZulValue ImmCharAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(8, val)), id_char};
}

bool ImmCharAST::is_const() {
    return true;
}

int ImmCharAST::get_typeid(ZulContext &zulctx) {
    return id_char;
}

ImmIntAST::ImmIntAST(long long int val) : val(val) {}

ZulValue ImmIntAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(64, val, true)), id_int};
}

bool ImmIntAST::is_const() {
    return true;
}

int ImmIntAST::get_typeid(ZulContext &zulctx) {
    return id_int;
}

ImmRealAST::ImmRealAST(double val) : val(val) {}

ZulValue ImmRealAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantFP::get(*zulctx.context, llvm::APFloat(val)), id_float};
}

bool ImmRealAST::is_const() {
    return true;
}

int ImmRealAST::get_typeid(ZulContext &zulctx) {
    return id_float;
}

ImmStrAST::ImmStrAST(string val) : val(std::move(val)) {}

ZulValue ImmStrAST::code_gen(ZulContext &zulctx) {
    return {zulctx.builder.CreateGlobalString(val, "", 0, zulctx.module.get()), id_char + TYPE_COUNTS};
}

bool ImmStrAST::is_const() {
    return true;
}

int ImmStrAST::get_typeid(ZulContext &zulctx) {
    return id_char + TYPE_COUNTS;
}
