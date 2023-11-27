#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <unordered_map>

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include "System.h"
#include "Utility.h"
#include "Lexer.h"

struct AST;

using LocalVarMap = std::unordered_map<std::string, std::pair<llvm::AllocaInst *, int>>;
using GlobalVarMap = std::map<std::string, std::pair<llvm::GlobalVariable *, int>>;

struct ZulValue {
    llvm::Value *value;
    int type_id;

    operator std::pair<llvm::Value *, int>() {
        return {value, type_id};
    }
};

static ZulValue nullzul{nullptr, -1};

struct ZulContext {
    llvm::LLVMContext context;
    llvm::Module module{"zul", context};
    llvm::IRBuilder<> builder{context};
    LocalVarMap local_var_map;
    GlobalVarMap global_var_map;

    ZulContext() {
        local_var_map.reserve(50);
    }
};

template<typename T>
struct Capture {
    T value;
    std::pair<int, int> loc;
    int word_size;

    Capture(T value, std::pair<int, int> loc, int word_size) :
            value(std::move(value)), loc(std::move(loc)), word_size(word_size) {}

    Capture(Capture<T> &&other) noexcept {
        value = std::move(other.value);
        loc = std::move(other.loc);
        word_size = other.word_size;
    }
};

template<typename T>
Capture<T> make_capture(T value, Lexer &lexer) {
    return Capture(std::move(value), lexer.get_token_start_loc(), lexer.get_word().size());
}

static bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id) {
    bool cast = true;
    if (dest_type_id == FLOAT_TYPEID) {
        target.value = zulctx.builder.CreateSIToFP(target.value, llvm::Type::getDoubleTy(zulctx.context));
    } else if (dest_type_id < FLOAT_TYPEID) {
        auto dest_type = get_llvm_type(zulctx.context, dest_type_id);
        if (target.type_id == FLOAT_TYPEID) {
            target.value = zulctx.builder.CreateFPToSI(target.value, dest_type);
        } else {
            target.value = zulctx.builder.CreateIntCast(target.value, dest_type, true);
        }
    } else {
        cast = false;
    }
    return cast;
}

static llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op) {
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

static llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op) {
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

static bool is_cmp(Token op) {
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

struct AST {
public:
    virtual ~AST() = default;

    virtual ZulValue code_gen(ZulContext &zulctx) = 0;
};

struct FuncProtoAST {
    std::string name;
    int return_type;
    std::vector<std::pair<std::string, int>> params;

    FuncProtoAST(std::string name, int return_type, std::vector<std::pair<std::string, int>> params) :
            name(std::move(name)), return_type(return_type), params(std::move(params)) {}

    llvm::Function *code_gen(ZulContext &zulctx) {
        std::vector<llvm::Type *> param_types;
        param_types.reserve(params.size());
        for (auto &param: params) {
            param_types.emplace_back(get_llvm_type(zulctx.context, param.second));
        }
        auto func_type = llvm::FunctionType::get(get_llvm_type(zulctx.context, return_type), param_types, false);
        auto llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, zulctx.module);
        int i = 0;
        for (auto &arg: llvm_func->args()) {
            arg.setName(params[i++].first);
        }
        return llvm_func;
    }
};

struct ImmIntAST : public AST {
    long long val;

    explicit ImmIntAST(long long val) : val(val) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        return {llvm::ConstantInt::get(zulctx.context, llvm::APInt(64, val, true)), FLOAT_TYPEID - 1};
    }
};

struct ImmRealAST : public AST {
    double val;

    explicit ImmRealAST(double val) : val(val) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        return {llvm::ConstantFP::get(zulctx.context, llvm::APFloat(val)), FLOAT_TYPEID};
    }
};

struct VariableAST : public AST {
    Capture<std::string> name;

    explicit VariableAST(Capture<std::string> name) : name(std::move(name)) {}

    std::pair<llvm::Value *, int> get_origin_value(ZulContext &zulctx) {
        if (zulctx.local_var_map.contains(name.value))
            return zulctx.local_var_map[name.value];
        if (zulctx.global_var_map.contains(name.value))
            return zulctx.global_var_map[name.value];
        System::logger.log_error(name.loc, name.word_size, {"\"", name.value, "\" 는 존재하지 않는 변수입니다"});
        return nullzul;
    }

    ZulValue code_gen(ZulContext &zulctx) override {
        std::pair<llvm::Value *, int> value = get_origin_value(zulctx);
        if (!value.first)
            return nullzul;
        value.first = zulctx.builder.CreateLoad(get_llvm_type(zulctx.context, value.second), value.first);
        return {value.first, value.second};
    }
};

struct VariableDeclAST : public AST {
    Capture<std::string> name;
    int type = -1;
    std::unique_ptr<AST> body = nullptr;

    VariableDeclAST(Capture<std::string> name, int type) :
            name(std::move(name)), type(type) {}

    VariableDeclAST(Capture<std::string> name, std::unique_ptr<AST> body) :
            name(std::move(name)), body(std::move(body)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        ZulValue init_val(nullptr, type);
        bool is_exist = zulctx.global_var_map.contains(name.value) || zulctx.local_var_map.contains(name.value);
        if (body) { //대입식이 있으면 식 먼저 생성
            auto result = body->code_gen(zulctx);
            if (result.value == nullptr)
                return nullzul;
            init_val.value = result.value;
            init_val.type_id = type = result.type_id;
        } else if (is_exist) { //대입식이 아닌데 (선언문인데) 이미 존재하는 변수면
            System::logger.log_error(name.loc, name.word_size, "변수가 재정의되었습니다");
            return nullzul;
        }
        llvm::Value *target;
        if (is_exist) { //이미 존재하는 변수인 경우
            std::pair<llvm::Value *, int> origin; //원본 변수를 가져옴
            if (zulctx.local_var_map.contains(name.value)) {
                origin = zulctx.local_var_map[name.value];
            } else {
                origin = zulctx.global_var_map[name.value];
            }
            //대입식과 원본 변수의 타입이 일치하지 않으면 대입식에 캐스팅 시도
            if (origin.second != type && !create_cast(zulctx, init_val, origin.second)) {
                System::logger.log_error(name.loc, name.word_size,
                                         {"타입이 일치하지 않습니다. 대입 연산식의 타입 \"",
                                          std::to_string(type), "\" 에서 변수의 타입 \"", std::to_string(origin.second),
                                          "\" 로 캐스팅 할 수 없습니다"});
                return nullzul;
            }
            target = origin.first;
        } else { //새로운 변수면 스택 할당 구문 추가
            auto func = zulctx.builder.GetInsertBlock()->getParent();
            llvm::IRBuilder<> temp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
            auto alloca = temp_builder.CreateAlloca(get_llvm_type(zulctx.context, type), nullptr, name.value);
            zulctx.local_var_map.emplace(name.value, std::make_pair(alloca, type));
            target = alloca;
        }
        if (body)
            zulctx.builder.CreateStore(init_val.value, target);
        return {target, type};
    }
};

struct VariableAssnAST : public AST {
    std::unique_ptr<VariableAST> target;
    Capture<Token> op;
    std::unique_ptr<AST> body;

    VariableAssnAST(std::unique_ptr<VariableAST> target, Capture<Token> op, std::unique_ptr<AST> body) :
            target(std::move(target)), op(std::move(op)), body(std::move(body)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        static std::unordered_map<Token, Token> assn_op_map = {
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

        if (!target_value.value || !body_value.value ||
            target_value.type_id > FLOAT_TYPEID || body_value.type_id > FLOAT_TYPEID)
            return nullzul;

        if (target_value.type_id != body_value.type_id && !create_cast(zulctx, body_value, target_value.type_id)) {
            System::logger.log_error(target->name.loc, target->name.word_size,
                                     {"대입 연산식의 타입 \"",
                                      std::to_string(target_value.type_id), "\" 에서 변수의 타입 \"",
                                      std::to_string(body_value.type_id),
                                      "\" 로 캐스팅 할 수 없습니다"});
            return nullzul;
        }
        auto prac_op = Capture(assn_op_map[op.value], op.loc, op.word_size);
        llvm::Value *result;
        if (target_value.type_id < FLOAT_TYPEID) {
            result = create_int_operation(zulctx, target_value.value, body_value.value, prac_op);
        } else {
            result = create_float_operation(zulctx, target_value.value, body_value.value, prac_op);
        }
        if (!result)
            return nullzul;
        result = zulctx.builder.CreateStore(result, target->get_origin_value(zulctx).first);
        return {result, target_value.type_id};
    }
};

struct BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Capture<Token> op;

    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Capture<Token> op) : left(std::move(left)),
                                                                                         right(std::move(right)),
                                                                                         op(std::move(op)) {}


    ZulValue code_gen(ZulContext &zulctx) override {
        auto lhs = left->code_gen(zulctx);
        auto rhs = right->code_gen(zulctx);
        if (!lhs.value || !rhs.value)
            return nullzul;
        if (lhs.type_id > FLOAT_TYPEID || rhs.type_id > FLOAT_TYPEID) { //연산자 오버로딩 지원 하게되면 변경
            return nullzul;
        }
        int calc_type = lhs.type_id;
        if (lhs.type_id > rhs.type_id) {
            calc_type = lhs.type_id;
            if (!create_cast(zulctx, rhs, lhs.type_id)) {
                System::logger.log_error(op.loc, op.word_size,
                                         {"우측항의 타입 \"",
                                          std::to_string(rhs.type_id), "\" 에서 좌측항의 타입 \"",
                                          std::to_string(lhs.type_id),
                                          "\" 로 캐스팅 할 수 없습니다"});
                return nullzul;
            }
        } else if (lhs.type_id < rhs.type_id) {
            calc_type = rhs.type_id;
            if (!create_cast(zulctx, lhs, rhs.type_id)) {
                System::logger.log_error(op.loc, op.word_size,
                                         {"좌측항의 타입 \"",
                                          std::to_string(lhs.type_id), "\" 에서 우측항의 타입 \"",
                                          std::to_string(rhs.type_id),
                                          "\" 로 캐스팅 할 수 없습니다"});
                return nullzul;
            }
        }
        llvm::Value *ret;
        if (calc_type < FLOAT_TYPEID) {
            ret = create_int_operation(zulctx, lhs.value, rhs.value, op);
        } else {
            ret = create_float_operation(zulctx, lhs.value, rhs.value, op);
        }
        calc_type = is_cmp(op.value) ? BOOL_TYPEID : calc_type;
        return {ret, calc_type};
    }
};

struct UnaryOpAST : public AST {
    std::unique_ptr<AST> body;
    Capture<Token> op;

    UnaryOpAST(std::unique_ptr<AST> body, Capture<Token> op) : body(std::move(body)), op(std::move(op)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        auto body_value = body->code_gen(zulctx);
        auto zero = get_const_zero(body_value.value->getType(), body_value.type_id);
        if (!body_value.value)
            return nullzul;
        if (body_value.type_id > FLOAT_TYPEID) {
            System::logger.log_error(op.loc, op.word_size, "단항 연산자를 적용할 수 없습니다");
            return nullzul;
        }
        switch (op.value) {
            case tok_add:
                break;
            case tok_sub:
                if (body_value.type_id < FLOAT_TYPEID)
                    return {zulctx.builder.CreateSub(zero, body_value.value), body_value.type_id};
                else
                    return {zulctx.builder.CreateFSub(zero, body_value.value), body_value.type_id};
            case tok_not:
                if (body_value.type_id < FLOAT_TYPEID)
                    return {zulctx.builder.CreateICmpEQ(zero, body_value.value), body_value.type_id};
                else
                    return {zulctx.builder.CreateFCmpOEQ(zero,body_value.value), body_value.type_id};
            case tok_bitnot:
                if (body_value.type_id == FLOAT_TYPEID) {
                    System::logger.log_error(op.loc, op.word_size, "단항 '~' 연산자를 적용할 수 없습니다");
                    return nullzul;
                }
                return {zulctx.builder.CreateNot(body_value.value), body_value.type_id};
            default:
                System::logger.log_error(op.loc, op.word_size, "올바른 단항 연산자가 아닙니다");
                return nullzul;
        }
        return body_value;
    }
};

struct FuncCallAST : public AST {
    FuncProtoAST *proto;
    std::vector<Capture<std::unique_ptr<AST>>> args;

    FuncCallAST(FuncProtoAST *proto,
                std::vector<Capture<std::unique_ptr<AST>>> args)
            : proto(proto), args(std::move(args)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        auto target_func = zulctx.module.getFunction(proto->name);
        std::vector<llvm::Value *> arg_values;
        bool has_error = false;
        arg_values.reserve(args.size());
        for (int i = 0; i < args.size(); i++) {
            auto arg = args[i].value->code_gen(zulctx);
            if (arg.type_id != proto->params[i].second && !create_cast(zulctx, arg, proto->params[i].second)) {
                //arg와 param의 타입이 맞지 않으면 캐스팅 시도
                System::logger.log_error(args[i].loc, args[i].word_size, {
                        "인자의 타입 \"", std::to_string(arg.type_id), "\" 에서 매개변수의 타입 \"",
                        std::to_string(proto->params[i].second), "\" 로 캐스팅 할 수 없습니다"});
                has_error = true;
            }
            arg_values.push_back(arg.value);
        }
        if (has_error)
            return nullzul;
        return {zulctx.builder.CreateCall(target_func, arg_values), proto->return_type};
    }
};

#endif //ZULLANG_AST_H
