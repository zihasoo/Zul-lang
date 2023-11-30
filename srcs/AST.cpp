#include "AST.h"

#include <utility>

using namespace std;
using namespace llvm;

static ZulValue nullzul{nullptr, -1};

ZulContext::ZulContext() {
    local_var_map.reserve(50);
}

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id) {
    bool cast = true;
    if (dest_type_id == FLOAT_TYPEID) {
        target.first = zulctx.builder.CreateSIToFP(target.first, llvm::Type::getDoubleTy(*zulctx.context));
    } else if (dest_type_id < FLOAT_TYPEID) {
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
            return true;
        default:
            return false;
    }
}

FuncProtoAST::FuncProtoAST(string name, int return_type, vector<pair<string, int>> params, bool has_body) :
        name(std::move(name)), return_type(return_type), params(std::move(params)), has_body(has_body) {}

llvm::Function *FuncProtoAST::code_gen(ZulContext &zulctx) {
    vector<llvm::Type *> param_types;
    param_types.reserve(params.size());
    for (auto &param: params) {
        param_types.emplace_back(get_llvm_type(*zulctx.context, param.second));
    }
    auto func_type = llvm::FunctionType::get(get_llvm_type(*zulctx.context, return_type), param_types, false);
    auto llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, *zulctx.module);
    int i = 0;
    for (auto &arg: llvm_func->args()) {
        arg.setName(params[i++].first);
    }
    return llvm_func;
}

FuncRetAST::FuncRetAST(std::unique_ptr<AST> body, Capture<int> return_type) :
        body(std::move(body)), return_type(std::move(return_type)) {}

ZulValue FuncRetAST::code_gen(ZulContext &zulctx) {
    auto body_value = body->code_gen(zulctx);
    if (!body_value.first)
        return nullzul;
    if (return_type.value != body_value.second && !create_cast(zulctx, body_value, return_type.value)) {
        System::logger.log_error(return_type.loc, return_type.word_size,
                                 {"리턴 타입이 일치하지 않습니다. 반환 구문의 타입 \"", type_name_map[body_value.second],
                                  "\" 에서 리턴 타입 \"", type_name_map[return_type.value], "\" 로 캐스팅 할 수 없습니다"});
    }

    auto ret = zulctx.builder.CreateRet(body_value.first);
    return {ret, body_value.second};
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

VariableAST::VariableAST(Capture<string> name) : name(std::move(name)) {}

pair<llvm::Value *, int> VariableAST::get_origin_value(ZulContext &zulctx) {
    if (zulctx.local_var_map.contains(name.value))
        return zulctx.local_var_map[name.value];
    if (zulctx.global_var_map.contains(name.value))
        return zulctx.global_var_map[name.value];
    System::logger.log_error(name.loc, name.word_size, {"\"", name.value, "\" 는 존재하지 않는 변수입니다"});
    return nullzul;
}

ZulValue VariableAST::code_gen(ZulContext &zulctx) {
    pair<llvm::Value *, int> value = get_origin_value(zulctx);
    if (!value.first)
        return nullzul;
    value.first = zulctx.builder.CreateLoad(get_llvm_type(*zulctx.context, value.second), value.first);
    return {value.first, value.second};
}

VariableDeclAST::VariableDeclAST(Capture<string> name, int type) :
        name(std::move(name)), type(type) {}

VariableDeclAST::VariableDeclAST(Capture<string> name, unique_ptr<AST> body) :
        name(std::move(name)), body(std::move(body)) {}

ZulValue VariableDeclAST::code_gen(ZulContext &zulctx) {
    ZulValue init_val(nullptr, type);
    bool is_exist = zulctx.global_var_map.contains(name.value) || zulctx.local_var_map.contains(name.value);
    if (body) { //대입식이 있으면 식 먼저 생성
        auto result = body->code_gen(zulctx);
        if (result.first == nullptr)
            return nullzul;
        init_val.first = result.first;
        init_val.second = type = result.second;
    } else if (is_exist) { //대입식이 아닌데 (선언문인데) 이미 존재하는 변수면
        System::logger.log_error(name.loc, name.word_size, "변수가 재정의되었습니다");
        return nullzul;
    }
    llvm::Value *target;
    if (is_exist) { //이미 존재하는 변수인 경우
        pair<llvm::Value *, int> origin; //원본 변수를 가져옴
        if (zulctx.local_var_map.contains(name.value)) {
            origin = zulctx.local_var_map[name.value];
        } else {
            origin = zulctx.global_var_map[name.value];
        }
        //대입식과 원본 변수의 타입이 일치하지 않으면 대입식에 캐스팅 시도
        if (origin.second != type && !create_cast(zulctx, init_val, origin.second)) {
            System::logger.log_error(name.loc, name.word_size,
                                     {"타입이 일치하지 않습니다. 대입 연산식의 타입 \"",
                                      type_name_map[type], "\" 에서 변수의 타입 \"", type_name_map[origin.second],
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
        target = origin.first;
    } else { //새로운 변수면 스택 할당 구문 추가
        auto func = zulctx.builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> temp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
        auto alloca_val = temp_builder.CreateAlloca(get_llvm_type(*zulctx.context, type), nullptr, name.value);
        zulctx.local_var_map.emplace(name.value, make_pair(alloca_val, type));
        target = alloca_val;
    }
    if (body)
        zulctx.builder.CreateStore(init_val.first, target);
    return {target, type};
}

VariableAssnAST::VariableAssnAST(unique_ptr<VariableAST> target, Capture<Token> op, unique_ptr<AST> body) :
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
    auto prac_op = Capture(assn_op_map[op.value], op.loc, op.word_size);
    llvm::Value *result;
    if (target_value.second < FLOAT_TYPEID) {
        result = create_int_operation(zulctx, target_value.first, body_value.first, prac_op);
    } else {
        result = create_float_operation(zulctx, target_value.first, body_value.first, prac_op);
    }
    if (!result)
        return nullzul;
    result = zulctx.builder.CreateStore(result, target->get_origin_value(zulctx).first);
    return {result, target_value.second};
}

BinOpAST::BinOpAST(unique_ptr<AST> left, unique_ptr<AST> right, Capture<Token> op) : left(std::move(left)),
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

UnaryOpAST::UnaryOpAST(unique_ptr<AST> body, Capture<Token> op) : body(std::move(body)), op(std::move(op)) {}

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

FuncCallAST::FuncCallAST(FuncProtoAST &proto, vector<Capture<unique_ptr<AST>>> args)
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
        if (arg.second != proto.params[i].second && !create_cast(zulctx, arg, proto.params[i].second)) {
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
