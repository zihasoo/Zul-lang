#include "AST.h"

#include <utility>

using namespace std;
using namespace llvm;

ZulValue nullzul{nullptr, -1};

ZulContext::ZulContext() {
    local_var_map.reserve(50);
}

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id) {
    bool cast = true;
    if (target.second < 0)
        return false;
    if (dest_type_id == FLOAT_TYPEID) {
        target.first = zulctx.builder.CreateSIToFP(target.first, llvm::Type::getDoubleTy(*zulctx.context));
    } else if (BOOL_TYPEID <= dest_type_id && dest_type_id < FLOAT_TYPEID) {
        auto dest_type = get_llvm_type(*zulctx.context, dest_type_id);
        if (target.second == FLOAT_TYPEID) {
            target.first = zulctx.builder.CreateFPToSI(target.first, dest_type);
        } else {
            target.first = zulctx.builder.CreateIntCast(target.first, dest_type, target.second != BOOL_TYPEID);
        }
    } else {
        cast = false;
    }
    return cast;
}

llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op) {
    switch (op.value) {
        case tok_add:
            return zulctx.builder.CreateAdd(lhs, rhs);
        case tok_sub:
            return zulctx.builder.CreateSub(lhs, rhs);
        case tok_mul:
            return zulctx.builder.CreateMul(lhs, rhs);
        case tok_div:
            return zulctx.builder.CreateSDiv(lhs, rhs);
        case tok_mod:
            return zulctx.builder.CreateSRem(lhs, rhs);
        case tok_bitand:
            return zulctx.builder.CreateAnd(lhs, rhs);
        case tok_bitor:
            return zulctx.builder.CreateOr(lhs, rhs);
        case tok_bitxor:
            return zulctx.builder.CreateXor(lhs, rhs);
        case tok_lshift:
            return zulctx.builder.CreateShl(lhs, rhs);
        case tok_rshift:
            return zulctx.builder.CreateAShr(lhs, rhs); //C++ Signed Shift Right 를 따름
        case tok_eq:
            return zulctx.builder.CreateICmpEQ(lhs, rhs);
        case tok_ineq:
            return zulctx.builder.CreateICmpNE(lhs, rhs);
        case tok_gt:
            return zulctx.builder.CreateICmpSGT(lhs, rhs);
        case tok_gteq:
            return zulctx.builder.CreateICmpSGE(lhs, rhs);
        case tok_lt:
            return zulctx.builder.CreateICmpSLT(lhs, rhs);
        case tok_lteq:
            return zulctx.builder.CreateICmpSLE(lhs, rhs);
        case tok_and:
        case tok_or:
            //short circuit 어떻게 구현?
        default:
            System::logger.log_error(op.loc, op.word_size, "해당 연산자를 수 타입에 적용할 수 없습니다");
            return nullptr;
    }
}

llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op) {
    switch (op.value) {
        case tok_add:
            return zulctx.builder.CreateFAdd(lhs, rhs);
        case tok_sub:
            return zulctx.builder.CreateFSub(lhs, rhs);
        case tok_mul:
            return zulctx.builder.CreateFMul(lhs, rhs);
        case tok_div:
            return zulctx.builder.CreateFDiv(lhs, rhs);
        case tok_mod:
            return zulctx.builder.CreateFRem(lhs, rhs);
        case tok_eq:
            return zulctx.builder.CreateFCmpOEQ(lhs, rhs);
        case tok_ineq:
            return zulctx.builder.CreateFCmpONE(lhs, rhs);
        case tok_gt:
            return zulctx.builder.CreateFCmpOGT(lhs, rhs);
        case tok_gteq:
            return zulctx.builder.CreateFCmpOGE(lhs, rhs);
        case tok_lt:
            return zulctx.builder.CreateFCmpOLT(lhs, rhs);
        case tok_lteq:
            return zulctx.builder.CreateFCmpOLE(lhs, rhs);
        default:
            System::logger.log_error(op.loc, op.word_size, "해당 연산자를 소수 타입에 적용할 수 없습니다");
            return nullptr;
    }
}

bool is_cmp(Token op) {
    switch (op) {
        case tok_eq:
        case tok_ineq:
        case tok_gt:
        case tok_gteq:
        case tok_lt:
        case tok_lteq:
            return true;
        default:
            return false;
    }
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
    auto llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, *zulctx.module);
    int i = 0;
    for (auto &arg: llvm_func->args()) {
        arg.setName(params[i++].first);
    }
    return llvm_func;
}

FuncRetAST::FuncRetAST(ASTPtr body, Capture<int> return_type) :
        body(std::move(body)), return_type(std::move(return_type)) {}

ZulValue FuncRetAST::code_gen(ZulContext &zulctx) {
    if (body) {
        auto body_value = body->code_gen(zulctx);
        if (!body_value.first)
            return nullzul;
        if (return_type.value != body_value.second && !create_cast(zulctx, body_value, return_type.value)) {
            System::logger.log_error(return_type.loc, return_type.word_size,
                                     {"리턴 타입이 일치하지 않습니다. 반환 구문의 타입 \"", type_name_map[body_value.second],
                                      "\" 에서 리턴 타입 \"", type_name_map[return_type.value], "\" 로 캐스팅 할 수 없습니다"});
        }
        if (zulctx.ret_count == 1) {
            zulctx.builder.CreateRet(body_value.first);
        } else {
            zulctx.builder.CreateStore(body_value.first, zulctx.return_var);
            zulctx.builder.CreateBr(zulctx.return_block);
        }
    } else {
        if (zulctx.ret_count == 1) {
            zulctx.builder.CreateRetVoid();
        } else {
            zulctx.builder.CreateBr(zulctx.return_block);
        }
    }
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

    if (init_body && !init_body->code_gen(zulctx).first)
        return nullzul;

    zulctx.builder.CreateBr(test_block);
    zulctx.builder.SetInsertPoint(test_block);
    if (test_body) {
        ZulValue test_cond = test_body->code_gen(zulctx);
        if (!test_cond.first)
            return nullzul;
        if (test_cond.second == FLOAT_TYPEID) {
            test_cond.first = zulctx.builder.CreateFCmpONE(test_cond.first,
                                                           llvm::ConstantFP::get(test_cond.first->getType(), 0));
        } else if (test_cond.second != BOOL_TYPEID) {
            auto zero = get_const_zero(*zulctx.context, test_cond.second);
            if (zero == nullptr) //bool형으로 캐스팅 불가능한 경우
                return nullzul;
            test_cond.first = zulctx.builder.CreateICmpNE(test_cond.first, zero);
        }
        zulctx.builder.CreateCondBr(test_cond.first, start_block, end_block);
    } else {
        zulctx.builder.CreateBr(start_block);
    }

    zulctx.builder.SetInsertPoint(start_block);
    for (auto &ast: loop_body) {
        ast->code_gen(zulctx);
    }
    zulctx.builder.CreateBr(update_block);

    zulctx.builder.SetInsertPoint(update_block);
    if (update_body && !update_body->code_gen(zulctx).first)
        return nullzul;
    zulctx.builder.CreateBr(test_block);

    zulctx.builder.SetInsertPoint(end_block);
    zulctx.loop_update_stack.pop();
    zulctx.loop_end_stack.pop();
    return nullzul;
}

ZulValue ContinueAST::code_gen(ZulContext &zulctx) {
    zulctx.builder.CreateBr(zulctx.loop_update_stack.top());
    return nullzul;
}

ZulValue BreakAST::code_gen(ZulContext &zulctx) {
    zulctx.builder.CreateBr(zulctx.loop_end_stack.top());
    return nullzul;
}

ImmBoolAST::ImmBoolAST(bool val) : val(val) {}

ZulValue ImmBoolAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(1, val)), BOOL_TYPEID};
}

ImmCharAST::ImmCharAST(char val) : val(val) {}

ZulValue ImmCharAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(8, val)), BOOL_TYPEID + 1};
}

ImmIntAST::ImmIntAST(long long int val) : val(val) {}

ZulValue ImmIntAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantInt::get(*zulctx.context, llvm::APInt(64, val, true)), FLOAT_TYPEID - 1};
}

ImmRealAST::ImmRealAST(double val) : val(val) {}

ZulValue ImmRealAST::code_gen(ZulContext &zulctx) {
    return {llvm::ConstantFP::get(*zulctx.context, llvm::APFloat(val)), FLOAT_TYPEID};
}

ImmStrAST::ImmStrAST(string val) : val(std::move(val)) {}

ZulValue ImmStrAST::code_gen(ZulContext &zulctx) {
    return {zulctx.builder.CreateGlobalStringPtr(val), 4};
}

VariableAST::VariableAST(string name) : name(std::move(name)) {}

pair<llvm::Value *, int> VariableAST::get_origin_value(ZulContext &zulctx) const {
    if (zulctx.local_var_map.contains(name))
        return zulctx.local_var_map[name];
    if (zulctx.global_var_map.contains(name))
        return zulctx.global_var_map[name];
    return nullzul;
}

ZulValue VariableAST::code_gen(ZulContext &zulctx) {
    pair<llvm::Value *, int> value = get_origin_value(zulctx);
    if (!value.first)
        return nullzul;
    value.first = zulctx.builder.CreateLoad(get_llvm_type(*zulctx.context, value.second), value.first);
    return value;
}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, int type) :
        name(std::move(name)), type(type) {}

VariableDeclAST::VariableDeclAST(Capture<std::string> name, ASTPtr body) :
        name(std::move(name)), body(std::move(body)) {}

ZulValue VariableDeclAST::code_gen(ZulContext &zulctx) {
    Value *init_val = nullptr;
    if (body) { //대입식이 있으면 식 먼저 생성
        auto result = body->code_gen(zulctx);
        if (result.first == nullptr)
            return nullzul;
        init_val = result.first;
        type = result.second;
    }
    if (type == -1) {
        System::logger.log_error(name.loc, name.word_size, {"\"", type_name_map[type], "\" 타입의 변수를 생성할 수 없습니다"});
        return nullzul;
    }
    auto func = zulctx.builder.GetInsertBlock()->getParent();
    llvm::IRBuilder<> entry_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    auto alloca_val = entry_builder.CreateAlloca(get_llvm_type(*zulctx.context, type), nullptr, name.value);
    zulctx.local_var_map[name.value] = make_pair(alloca_val, type);

    if (body)
        zulctx.builder.CreateStore(init_val, alloca_val);
    return {alloca_val, type};
}

VariableAssnAST::VariableAssnAST(unique_ptr<VariableAST> target, Capture<Token> op, ASTPtr body) :
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
        target_value.second > FLOAT_TYPEID || body_value.second > FLOAT_TYPEID) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  type_name_map[target_value.second], "\" 과 변수의 타입 \"",
                                  type_name_map[body_value.second],
                                  "\" 사이에 적절한 연산자 오버로드가 없습니다"});
        return nullzul;
    }

    if (target_value.second != body_value.second && !create_cast(zulctx, body_value, target_value.second)) {
        System::logger.log_error(op.loc, op.word_size,
                                 {"대입 연산식의 타입 \"",
                                  type_name_map[target_value.second], "\" 에서 변수의 타입 \"",
                                  type_name_map[body_value.second],
                                  "\" 로 캐스팅 할 수 없습니다"});
        return nullzul;
    }
    llvm::Value *result;
    if (op.value == tok_assn) {
        result = body_value.first;
    } else {
        auto prac_op = Capture(assn_op_map[op.value], op.loc, op.word_size);
        if (target_value.second < FLOAT_TYPEID) {
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

ZulValue BinOpAST::code_gen(ZulContext &zulctx) {
    auto lhs = left->code_gen(zulctx);
    auto rhs = right->code_gen(zulctx);
    if (!lhs.first || !rhs.first)
        return nullzul;
    if (lhs.second > FLOAT_TYPEID || rhs.second > FLOAT_TYPEID) { //연산자 오버로딩 지원 하게되면 변경
        System::logger.log_error(op.loc, op.word_size, {"좌측항의 타입 \"",
                                                        type_name_map[lhs.second], "\" 과 우측항의 타입 \"",
                                                        type_name_map[rhs.second],
                                                        "\" 사이에 적절한 연산자 오버로드가 없습니다"});
        return nullzul;
    }
    int calc_type = lhs.second;
    if (lhs.second > rhs.second) {
        calc_type = lhs.second;
        if (!create_cast(zulctx, rhs, lhs.second)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"우측항의 타입 \"",
                                      type_name_map[rhs.second], "\" 에서 좌측항의 타입 \"",
                                      type_name_map[lhs.second],
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    } else if (lhs.second < rhs.second) {
        calc_type = rhs.second;
        if (!create_cast(zulctx, lhs, rhs.second)) {
            System::logger.log_error(op.loc, op.word_size,
                                     {"좌측항의 타입 \"",
                                      type_name_map[lhs.second], "\" 에서 우측항의 타입 \"",
                                      type_name_map[rhs.second],
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
    }
    llvm::Value *ret;
    if (calc_type < FLOAT_TYPEID) {
        ret = create_int_operation(zulctx, lhs.first, rhs.first, op);
    } else {
        ret = create_float_operation(zulctx, lhs.first, rhs.first, op);
    }
    calc_type = is_cmp(op.value) ? BOOL_TYPEID : calc_type;
    return {ret, calc_type};
}

UnaryOpAST::UnaryOpAST(ASTPtr body, Capture<Token> op) : body(std::move(body)), op(std::move(op)) {}

ZulValue UnaryOpAST::code_gen(ZulContext &zulctx) {
    auto body_value = body->code_gen(zulctx);
    auto zero = get_const_zero(body_value.first->getType(), body_value.second);
    if (!body_value.first)
        return nullzul;
    if (body_value.second > FLOAT_TYPEID) {
        System::logger.log_error(op.loc, op.word_size, "단항 연산자를 적용할 수 없습니다");
        return nullzul;
    }
    switch (op.value) {
        case tok_add:
            break;
        case tok_sub:
            if (body_value.second < FLOAT_TYPEID)
                return {zulctx.builder.CreateSub(zero, body_value.first), body_value.second};
            else
                return {zulctx.builder.CreateFSub(zero, body_value.first), body_value.second};
        case tok_not:
            if (body_value.second < FLOAT_TYPEID)
                return {zulctx.builder.CreateICmpEQ(zero, body_value.first), body_value.second};
            else
                return {zulctx.builder.CreateFCmpOEQ(zero, body_value.first), body_value.second};
        case tok_bitnot:
            if (body_value.second == FLOAT_TYPEID) {
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

FuncCallAST::FuncCallAST(FuncProtoAST &proto, vector<Capture<ASTPtr>> args)
        : proto(proto), args(std::move(args)) {}

ZulValue FuncCallAST::code_gen(ZulContext &zulctx) {
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
                    "인자의 타입 \"", type_name_map[arg.second], "\" 에서 매개변수의 타입 \"",
                    type_name_map[proto.params[i].second], "\" 로 캐스팅 할 수 없습니다"});
            has_error = true;
        }
        arg_values.push_back(arg.first);
    }
    if (has_error)
        return nullzul;
    return {zulctx.builder.CreateCall(target_func, arg_values), proto.return_type};
}
