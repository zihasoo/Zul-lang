
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
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
using llvm::BasicBlock;
using llvm::cast;

bool ExprAST::is_const() {
    return false;
}

bool ExprAST::is_lvalue() {
    return false;
}

int ExprAST::get_typeid(ZulContext &zul_context) {
    return -1;
}

bool LValueAST::is_lvalue() {
    return true;
}

FuncProtoAST::FuncProtoAST(string name, int return_type, vector<pair<string, int>> params, bool has_body,
                           bool is_var_arg) :
        name(std::move(name)), return_type(return_type), params(std::move(params)), has_body(has_body),
        is_var_arg(is_var_arg) {}

llvm::Function *FuncProtoAST::code_gen(ZulContext &zul_context) const {
    vector<Type *> param_types;
    param_types.reserve(params.size());
    for (auto &param: params) {
        param_types.emplace_back(zul_context.id_to_type(param.second));
    }
    auto func_type = llvm::FunctionType::get(zul_context.id_to_type(return_type), param_types, is_var_arg);
    return llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, *zul_context.module);
}

FuncRetAST::FuncRetAST(ASTPtr body, Capture<int> return_type) :
        body(std::move(body)), return_type(std::move(return_type)) {}

ZulValue FuncRetAST::code_gen(ZulContext &zul_context) {
    ZulValue body_value = nullzul;
    if (body) {
        body_value = body->code_gen(zul_context);
        if (!body_value.value)
            return nullzul;
    }
    if (return_type.value == -1) {
        if (body && body_value.id != -1) {
            System::logger.log_error(return_type.loc, return_type.word_size,
                                     {"리턴 타입이 일치하지 않습니다. 함수의 반환 타입이 \"없음\" 이지만 \"",
                                      id_to_name(body_value.id), "\" 타입을 반환하고 있습니다"});
        }
        if (zul_context.ret_count == 1) {
            zul_context.builder.CreateRetVoid();
        } else {
            zul_context.builder.CreateBr(zul_context.return_block);
        }
    } else {
        if (return_type.value != body_value.id && !zul_context.create_cast(body_value, return_type.value)) {
            System::logger.log_error(return_type.loc, return_type.word_size,
                                     {"리턴 타입이 일치하지 않습니다. 반환 구문의 타입 \"", id_to_name(body_value.id),
                                      "\" 에서 리턴 타입 \"", id_to_name(return_type.value), "\" 로 캐스팅 할 수 없습니다"});
        } else if (zul_context.ret_count == 1) {
            zul_context.builder.CreateRet(body_value.value);
        } else {
            zul_context.builder.CreateStore(body_value.value, zul_context.return_var);
            zul_context.builder.CreateBr(zul_context.return_block);
        }
    }
    return {nullptr, ID_INTERRUPT};
}

int FuncRetAST::get_typeid(ZulContext &zul_context) {
    return return_type.value;
}

IfAST::IfAST(CondBodyPair if_pair, std::vector<CondBodyPair> elif_pair_list, std::vector<ASTPtr> else_body) :
        if_pair(std::move(if_pair)), elif_pair_list(std::move(elif_pair_list)), else_body(std::move(else_body)) {}

ZulValue IfAST::code_gen(ZulContext &zul_context) {
    auto prev_cond = if_pair.first->code_gen(zul_context);
    if (!prev_cond.value || !zul_context.to_boolean_expr(prev_cond))
        return nullzul;

    auto func = zul_context.builder.GetInsertBlock()->getParent();
    auto body_block = BasicBlock::Create(*zul_context.context, "if", func);
    auto merge_block = BasicBlock::Create(*zul_context.context, "merge");
    auto prev_block = zul_context.builder.GetInsertBlock();

    zul_context.builder.SetInsertPoint(body_block);
    bool interrupted = false;
    for (auto &ast: if_pair.second) {
        auto code = ast->code_gen(zul_context).id;
        if (code == ID_INTERRUPT) {
            interrupted = true;
            break;
        }
    }
    if (!interrupted)
        zul_context.builder.CreateBr(merge_block);

    for (auto &elif_pair: elif_pair_list) {
        auto elif_cond_block = BasicBlock::Create(*zul_context.context, "elif_cond", func);
        zul_context.builder.SetInsertPoint(prev_block);
        zul_context.builder.CreateCondBr(prev_cond.value, body_block, elif_cond_block);

        zul_context.builder.SetInsertPoint(elif_cond_block);
        prev_cond = elif_pair.first->code_gen(zul_context);
        if (!prev_cond.value || !zul_context.to_boolean_expr(prev_cond))
            return nullzul;

        body_block = BasicBlock::Create(*zul_context.context, "elif", func);
        zul_context.builder.SetInsertPoint(body_block);
        interrupted = false;
        for (auto &ast: elif_pair.second) {
            auto code = ast->code_gen(zul_context).id;
            if (code == ID_INTERRUPT) {
                interrupted = true;
                break;
            }
        }
        if (!interrupted)
            zul_context.builder.CreateBr(merge_block);
        prev_block = elif_cond_block;
    }
    if (else_body.empty()) {
        zul_context.builder.SetInsertPoint(prev_block);
        zul_context.builder.CreateCondBr(prev_cond.value, body_block, merge_block);
    } else {
        auto else_block = BasicBlock::Create(*zul_context.context, "else", func);
        zul_context.builder.SetInsertPoint(prev_block);
        zul_context.builder.CreateCondBr(prev_cond.value, body_block, else_block);

        zul_context.builder.SetInsertPoint(else_block);
        interrupted = false;
        for (auto &ast: else_body) {
            auto code = ast->code_gen(zul_context).id;
            if (code == ID_INTERRUPT) {
                interrupted = true;
                break;
            }
        }
        if (!interrupted)
            zul_context.builder.CreateBr(merge_block);
    }

    func->insert(func->end(), merge_block);
    zul_context.builder.SetInsertPoint(merge_block);
    return nullzul;
}

LoopAST::LoopAST(ASTPtr init_body, ASTPtr test_body, ASTPtr update_body, std::vector<ASTPtr> loop_body) :
        init_body(std::move(init_body)), test_body(std::move(test_body)), update_body(std::move(update_body)),
        loop_body(std::move(loop_body)) {}

ZulValue LoopAST::code_gen(ZulContext &zul_context) {
    auto func = zul_context.builder.GetInsertBlock()->getParent();
    auto test_block = BasicBlock::Create(*zul_context.context, "loop_test", func);
    auto start_block = BasicBlock::Create(*zul_context.context, "loop_start", func);
    auto update_block = BasicBlock::Create(*zul_context.context, "loop_update", func);
    auto end_block = BasicBlock::Create(*zul_context.context, "loop_end", func);
    zul_context.loop_update_stack.push(update_block);
    zul_context.loop_end_stack.push(end_block);
    Guard guard{[&zul_context]() { //리턴 할 때 자동으로 pop하도록 가드 생성
        zul_context.loop_update_stack.pop();
        zul_context.loop_end_stack.pop();
    }};

    if (init_body && !init_body->code_gen(zul_context).value)
        return nullzul;

    zul_context.builder.CreateBr(test_block);
    zul_context.builder.SetInsertPoint(test_block);
    if (test_body) {
        ZulValue test_cond = test_body->code_gen(zul_context);
        if (!test_cond.value || !zul_context.to_boolean_expr(test_cond))
            return nullzul;
        zul_context.builder.CreateCondBr(test_cond.value, start_block, end_block);
    } else {
        zul_context.builder.CreateBr(start_block);
    }

    bool interrupted = false;
    zul_context.builder.SetInsertPoint(start_block);
    for (auto &ast: loop_body) {
        auto code = ast->code_gen(zul_context);
        if (code.id == ID_INTERRUPT) {
            interrupted = true;
            break;
        }
    }
    if (!interrupted)
        zul_context.builder.CreateBr(update_block);

    zul_context.builder.SetInsertPoint(update_block);
    if (update_body && !update_body->code_gen(zul_context).value)
        return nullzul;
    zul_context.builder.CreateBr(test_block);

    zul_context.builder.SetInsertPoint(end_block);
    return nullzul;
}

ZulValue ContinueAST::code_gen(ZulContext &zul_context) {
    zul_context.builder.CreateBr(zul_context.loop_update_stack.top());
    return {nullptr, ID_INTERRUPT};
}

ZulValue BreakAST::code_gen(ZulContext &zul_context) {
    zul_context.builder.CreateBr(zul_context.loop_end_stack.top());
    return {nullptr, ID_INTERRUPT};
}

VariableAST::VariableAST(string name) : name(std::move(name)) {}

ZulValue VariableAST::get_origin_value(ZulContext &zul_context) {
    if (zul_context.local_var_map.contains(name))
        return zul_context.local_var_map[name];
    if (zul_context.global_var_map.contains(name))
        return zul_context.global_var_map[name];
    return nullzul;
}

ZulValue VariableAST::code_gen(ZulContext &zul_context) {
    auto value = get_origin_value(zul_context);
    if (!value.value)
        return nullzul;
    if (value.id < ID_MAX || value.id >= ID_MAX * 2) {
        value.value = zul_context.builder.CreateLoad(zul_context.id_to_type(value.id), value.value);
        if (value.id >= ID_MAX * 2)
            value.id -= ID_MAX;
    }
    return value;
}

int VariableAST::get_typeid(ZulContext &zul_context) {
    return get_origin_value(zul_context).id;
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, ZulContext &zul_context, int type, ASTPtr body) :
        name(std::move(name)), type(type), body(std::move(body)) {
    register_var(zul_context);
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, ZulContext &zul_context, ASTPtr body) :
        name(std::move(name)), type(-1), body(std::move(body)) {
    register_var(zul_context);
}

void VariableDeclAST::register_var(ZulContext &zul_context) {
    int t = (type == -1 ? body->get_typeid(zul_context) : type);
    zul_context.local_var_map.emplace(name.value, make_pair(nullptr, t)); //이름만 등록 해놓기
    if (!zul_context.scope_stack.empty()) { //만약 스코프 안에 있다면
        zul_context.scope_stack.top().push_back(name.value); //가장 가까운 스코프에 변수 등록
    }
}

ZulValue VariableDeclAST::code_gen(ZulContext &zul_context) {
    Value *init_val = nullptr;
    if (body) { //대입식이 있으면 식 먼저 생성
        auto result = body->code_gen(zul_context);
        if (!result.value)
            return nullzul;
        if (type != -1 && type != result.id && !zul_context.create_cast(result, type)) {
            System::logger.log_error(name.loc, name.word_size,
                                     {"대입 연산식의 타입 \"",
                                      id_to_name(result.id), "\" 에서 변수의 타입 \"",
                                      id_to_name(type),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
        if (type == -1) //타입 명시가 안됐을 때만
            type = result.id; //추론된 타입 적용
        init_val = result.value;
    }
    if (type == -1) {
        System::logger.log_error(name.loc, name.word_size, {"\"", id_to_name(type), "\" 타입의 변수를 생성할 수 없습니다"});
        return nullzul;
    }
    if (ID_MAX < type && type < ID_MAX * 2) //배열 타입일 경우 포인터로 변환함
        type += ID_MAX;
    auto func = zul_context.builder.GetInsertBlock()->getParent();
    llvm::IRBuilder<> entry_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    auto alloca_val = entry_builder.CreateAlloca(zul_context.id_to_type(type), nullptr, name.value);
    zul_context.local_var_map[name.value] = make_pair(alloca_val, type);

    if (body)
        zul_context.builder.CreateStore(init_val, alloca_val);
    return {alloca_val, type};
}

VariableAssnAST::VariableAssnAST(unique_ptr<LValueAST> target, Capture<Token> op, ASTPtr body) :
        target(std::move(target)), op(std::move(op)), body(std::move(body)) {}

ZulValue VariableAssnAST::code_gen(ZulContext &zul_context) {
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
    auto target_value = target->code_gen(zul_context);
    auto body_value = body->code_gen(zul_context);

    if (!target_value.value || !body_value.value ||
        target_value.id > ID_FLOAT || body_value.id > ID_FLOAT) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  id_to_name(target_value.id), "\" 와 변수의 타입 \"",
                                  id_to_name(body_value.id),
                                  "\" 는 연산이 불가능합니다"});
        return nullzul;
    }

    if (target_value.id != body_value.id && !zul_context.create_cast(body_value, target_value.id)) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  id_to_name(target_value.id), "\" 에서 변수의 타입 \"",
                                  id_to_name(body_value.id),
                                  "\" 로 캐스팅 할 수 없습니다"});
        return nullzul;
    }
    Value *result;
    if (op.value == tok_assn) {
        result = body_value.value;
    } else {
        auto prac_op = Capture(assn_op_map[op.value], op.loc, op.word_size);
        if (target_value.id < ID_FLOAT) {
            result = zul_context.create_int_operation(target_value.value, body_value.value, prac_op);
        } else {
            result = zul_context.create_float_operation(target_value.value, body_value.value, prac_op);
        }
        if (!result)
            return nullzul;
    }
    return {zul_context.builder.CreateStore(result, target->get_origin_value(zul_context).value), target_value.id};
}

BinOpAST::BinOpAST(ASTPtr left, ASTPtr right, Capture<Token> op) : left(std::move(left)),
                                                                   right(std::move(right)),
                                                                   op(std::move(op)) {}

ZulValue BinOpAST::short_circuit_code_gen(ZulContext &zul_context) const {
    auto lhs = left->code_gen(zul_context);
    if (!lhs.value)
        return nullzul;
    if (!zul_context.to_boolean_expr(lhs)) {
        System::logger.log_error(op.loc, op.word_size, "좌측항을 \"논리\" 자료형으로 캐스팅 할 수 없습니다");
        return nullzul;
    }

    auto origin_block = zul_context.builder.GetInsertBlock();
    auto func = zul_context.builder.GetInsertBlock()->getParent();
    auto sc_test = BasicBlock::Create(*zul_context.context, "sc_test", func);
    auto sc_end = BasicBlock::Create(*zul_context.context, "sc_end", func);

    if (op.value == tok_and)
        zul_context.builder.CreateCondBr(lhs.value, sc_test, sc_end);
    else
        zul_context.builder.CreateCondBr(lhs.value, sc_end, sc_test);

    zul_context.builder.SetInsertPoint(sc_test);
    auto rhs = right->code_gen(zul_context);
    if (!rhs.value)
        return nullzul;
    if (!zul_context.to_boolean_expr(rhs)) {
        System::logger.log_error(op.loc, op.word_size, "우측항을 \"논리\" 자료형으로 캐스팅 할 수 없습니다");
        return nullzul;
    }
    zul_context.builder.CreateBr(sc_end);
    auto last_block = zul_context.builder.GetInsertBlock();

    zul_context.builder.SetInsertPoint(sc_end);
    auto phi = zul_context.builder.CreatePHI(zul_context.id_to_type(0), 2);
    phi->addIncoming(ConstantInt::getBool(*zul_context.context, op.value == tok_or), origin_block);
    phi->addIncoming(rhs.value, last_block);
    return {phi, 0};
}

ZulValue BinOpAST::code_gen(ZulContext& zul_context) {
    if (op.value == tok_and || op.value == tok_or) {
        return short_circuit_code_gen(zul_context);
    }
    auto lhs = left->code_gen(zul_context);
    auto rhs = right->code_gen(zul_context);
    if (!lhs.value || !rhs.value)
        return nullzul;
    if (lhs.id > ID_FLOAT || rhs.id > ID_FLOAT) { //연산자 오버로딩 지원 하게되면 변경
        System::logger.log_error(op.loc, op.word_size, {"좌측항의 타입 \"",
                                                        id_to_name(lhs.id), "\" 와 우측항의 타입 \"",
                                                        id_to_name(rhs.id),
                                                        "\" 는 연산이 불가능합니다"});
        return nullzul;
    }
    int calc_type = lhs.id;
    if (lhs.id > rhs.id) {
        calc_type = lhs.id;
        if (!zul_context.create_cast(rhs, lhs.id)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"우측항의 타입 \"",
                                      id_to_name(rhs.id), "\" 에서 좌측항의 타입 \"",
                                      id_to_name(lhs.id),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    } else if (lhs.id < rhs.id) {
        calc_type = rhs.id;
        if (!zul_context.create_cast(lhs, rhs.id)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"좌측항의 타입 \"",
                                      id_to_name(lhs.id), "\" 에서 우측항의 타입 \"",
                                      id_to_name(rhs.id),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    }
    Value *ret;
    if (calc_type < ID_FLOAT) {
        ret = zul_context.create_int_operation(lhs.value, rhs.value, op);
    } else {
        ret = zul_context.create_float_operation(lhs.value, rhs.value, op);
    }
    calc_type = iscmp(op.value) ? ID_BOOL : calc_type;
    return {ret, calc_type};
}

bool BinOpAST::is_const() {
    return left->is_const() && right->is_const() && op.value != tok_and && op.value != tok_or;
}

int BinOpAST::get_typeid(ZulContext &zul_context) {
    if (op.value == tok_and || op.value == tok_or)
        return 0;
    int ltype = left->get_typeid(zul_context);
    int rtype = right->get_typeid(zul_context);
    if (ltype > ID_FLOAT || rtype > ID_FLOAT || ltype < ID_BOOL || rtype < ID_BOOL)
        return -1;
    return max(ltype, rtype);
}

UnaryOpAST::UnaryOpAST(ASTPtr body, Capture<Token> op) : body(std::move(body)), op(std::move(op)) {}

ZulValue UnaryOpAST::code_gen(ZulContext &zul_context) {
    auto body_value = body->code_gen(zul_context);
    auto zero = zul_context.zero_of(body_value.id);
    if (!body_value.value)
        return nullzul;
    if (body_value.id > ID_FLOAT) {
        System::logger.log_error(op.loc, op.word_size, "단항 연산자를 적용할 수 없습니다");
        return nullzul;
    }
    switch (op.value) {
        case tok_add:
            break;
        case tok_sub:
            if (body_value.id < ID_FLOAT)
                return {zul_context.builder.CreateSub(zero, body_value.value), body_value.id};
            else
                return {zul_context.builder.CreateFSub(zero, body_value.value), body_value.id};
        case tok_not:
            if (body_value.id < ID_FLOAT)
                return {zul_context.builder.CreateICmpEQ(zero, body_value.value), 0};
            else
                return {zul_context.builder.CreateFCmpOEQ(zero, body_value.value), 0};
        case tok_bitnot:
            if (body_value.id == ID_FLOAT) {
                System::logger.log_error(op.loc, op.word_size, "단항 '~' 연산자를 적용할 수 없습니다");
                return nullzul;
            }
            return {zul_context.builder.CreateNot(body_value.value), body_value.id};
        default:
            System::logger.log_error(op.loc, op.word_size, "올바른 단항 연산자가 아닙니다");
            return nullzul;
    }
    return body_value;
}

bool UnaryOpAST::is_const() {
    return body->is_const();
}

int UnaryOpAST::get_typeid(ZulContext &zul_context) {
    if (op.value == tok_not)
        return 0;
    return body->get_typeid(zul_context);
}

SubscriptAST::SubscriptAST(std::unique_ptr<VariableAST> target, Capture<ASTPtr> index) : target(std::move(target)),
                                                                                         index(std::move(index)) {}

ZulValue SubscriptAST::get_origin_value(ZulContext &zul_context) {
    ZulValue target_val;
    if (target->get_typeid(zul_context) >= ID_MAX * 2) {
        target_val = target->code_gen(zul_context);
    } else {
        target_val = target->get_origin_value(zul_context);
    }
    auto index_val = index.value->code_gen(zul_context);
    if (!target_val.value || !index_val.value)
        return nullzul;
    if (target_val.id < ID_MAX) {
        System::logger.log_error(index.loc, index.word_size, "'[]' 연산자를 사용할 수 없습니다. 배열이 아닙니다.");
        return nullzul;
    }
    if (index_val.id != 2) {
        System::logger.log_error(index.loc, index.word_size, "배열의 인덱스는 정수여야 합니다");
        return nullzul;
    }
    target_val.id -= ID_MAX;
    auto elm_type = zul_context.id_to_type(target_val.id);
    auto elm_ptr = zul_context.builder.CreateGEP(elm_type, target_val.value, {index_val.value});
    return {elm_ptr, target_val.id};
}

ZulValue SubscriptAST::code_gen(ZulContext &zul_context) {
    auto elm_ptr = get_origin_value(zul_context);
    auto loaded = zul_context.builder.CreateLoad(zul_context.id_to_type(elm_ptr.id), elm_ptr.value);
    return {loaded, elm_ptr.id};
}

int SubscriptAST::get_typeid(ZulContext &zul_context) {
    return target->get_typeid(zul_context) - ID_MAX;
}

FuncCallAST::FuncCallAST(FuncProtoAST &proto, vector<Capture<ASTPtr>> args)
        : proto(proto), args(std::move(args)) {}


std::string_view FuncCallAST::get_format_str(int type_id) {
    if (format_str_map.contains(type_id))
        return format_str_map[type_id];
    return "%p";
}

ZulValue FuncCallAST::handle_std_in(ZulContext &zul_context) {
    vector<Value *> arg_values;
    string format_str;
    auto s = args.size();
    bool has_error = false;
    arg_values.reserve(args.size());
    for (int i = 0; i < s; i++) {
        if (!args[i].value->is_lvalue()) {
            System::logger.log_error(args[i].loc, args[i].word_size, {"\"", STDIN_NAME, "\" 함수에는 좌측값만 올 수 있습니다"});
            has_error = true;
        }
        auto arg = cast<LValueAST>(args[i].value.get())->get_origin_value(zul_context);
        if (!arg.value)
            return nullzul;

        format_str.append(get_format_str(arg.id));
        if (i < s - 1)
            format_str.push_back(' ');
        arg_values.push_back(arg.value);
    }
    arg_values.insert(arg_values.begin(), zul_context.builder.CreateGlobalString(format_str));
    if (has_error)
        return nullzul;
    return {zul_context.builder.CreateCall(zul_context.module->getFunction("scanf"), arg_values), -1};
}

ZulValue FuncCallAST::handle_std_out(ZulContext &zul_context) {
    vector<Value *> arg_values;
    string format_str;
    auto s = args.size();
    arg_values.reserve(args.size());
    for (int i = 0; i < s; i++) {
        auto arg = args[i].value->code_gen(zul_context);
        if (!arg.value)
            return nullzul;
        if (arg.id == -1) {
            System::logger.log_error(args[i].loc, args[i].word_size, "\"없음\" 타입을 출력할 수 없습니다");
        }
        format_str.append(get_format_str(arg.id));
        if (i < s - 1)
            format_str.push_back(' ');
        arg_values.push_back(arg.value);
    }
    format_str.push_back('\n');
    arg_values.insert(arg_values.begin(), zul_context.builder.CreateGlobalString(format_str));
    return {zul_context.builder.CreateCall(zul_context.module->getFunction("printf"), arg_values), -1};
}

ZulValue FuncCallAST::code_gen(ZulContext &zul_context) {
    if (proto.name == STDIN_NAME)
        return handle_std_in(zul_context);
    if (proto.name == STDOUT_NAME)
        return handle_std_out(zul_context);
    auto target_func = zul_context.module->getFunction(proto.name);
    vector<llvm::Value *> arg_values;
    bool has_error = false;
    arg_values.reserve(args.size());
    for (int i = 0; i < args.size(); i++) {
        auto arg = args[i].value->code_gen(zul_context);
        if (!arg.value)
            return nullzul;
        if (i < proto.params.size() && arg.id != proto.params[i].second &&
            !zul_context.create_cast(arg, proto.params[i].second)) {
            //arg와 param의 타입이 맞지 않으면 캐스팅 시도
            System::logger.log_error(args[i].loc, args[i].word_size, {
                    "인자의 타입 \"", id_to_name(arg.id), "\" 에서 매개변수의 타입 \"",
                    id_to_name(proto.params[i].second), "\" 로 캐스팅 할 수 없습니다"});
            has_error = true;
        }
        arg_values.push_back(arg.value);
    }
    if (has_error)
        return nullzul;
    return {zul_context.builder.CreateCall(target_func, arg_values), proto.return_type};
}

int FuncCallAST::get_typeid(ZulContext &zul_context) {
    return proto.return_type;
}

unordered_map<int, string_view> FuncCallAST::format_str_map = {
        {ID_BOOL,              "%u"},
        {ID_CHAR,              "%c"},
        {ID_INT,               "%lld"},
        {ID_FLOAT,             "%lf"},
        {ID_CHAR + ID_MAX,     "%s"},
        {ID_CHAR + ID_MAX * 2, "%s"},
};

ImmBoolAST::ImmBoolAST(bool val) : val(val) {}

ZulValue ImmBoolAST::code_gen(ZulContext &zul_context) {
    return {ConstantInt::get(*zul_context.context, llvm::APInt(1, val)), ID_BOOL};
}

bool ImmBoolAST::is_const() {
    return true;
}

int ImmBoolAST::get_typeid(ZulContext &zul_context) {
    return ID_BOOL;
}

ImmCharAST::ImmCharAST(char val) : val(val) {}

ZulValue ImmCharAST::code_gen(ZulContext &zul_context) {
    return {ConstantInt::get(*zul_context.context, llvm::APInt(8, val)), ID_CHAR};
}

bool ImmCharAST::is_const() {
    return true;
}

int ImmCharAST::get_typeid(ZulContext &zul_context) {
    return ID_CHAR;
}

ImmIntAST::ImmIntAST(long long int val) : val(val) {}

ZulValue ImmIntAST::code_gen(ZulContext &zul_context) {
    return {ConstantInt::get(*zul_context.context, llvm::APInt(64, val, true)), ID_INT};
}

bool ImmIntAST::is_const() {
    return true;
}

int ImmIntAST::get_typeid(ZulContext &zul_context) {
    return ID_INT;
}

ImmRealAST::ImmRealAST(double val) : val(val) {}

ZulValue ImmRealAST::code_gen(ZulContext &zul_context) {
    return {ConstantFP::get(*zul_context.context, llvm::APFloat(val)), ID_FLOAT};
}

bool ImmRealAST::is_const() {
    return true;
}

int ImmRealAST::get_typeid(ZulContext &zul_context) {
    return ID_FLOAT;
}

ImmStrAST::ImmStrAST(string val) : val(std::move(val)) {}

ZulValue ImmStrAST::code_gen(ZulContext &zul_context) {
    return {zul_context.builder.CreateGlobalString(val, "", 0, zul_context.module.get()), ID_CHAR + ID_MAX};
}

bool ImmStrAST::is_const() {
    return true;
}

int ImmStrAST::get_typeid(ZulContext &zul_context) {
    return ID_CHAR + ID_MAX;
}
