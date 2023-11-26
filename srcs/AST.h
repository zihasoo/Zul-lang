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
};

struct ZulContext {
    llvm::LLVMContext context;
    llvm::Module module{"zul", context};
    llvm::IRBuilder<> builder{context};
    LocalVarMap local_var_map;
    GlobalVarMap global_var_map;
    std::unordered_map<std::string, ZulValue> arg_value_map;

    ZulContext() {
        local_var_map.reserve(30);
        arg_value_map.reserve(10);
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

struct AST {
public:
    virtual ~AST() = default;

    virtual ZulValue code_gen(ZulContext &zulctx) = 0;
};

struct FuncProtoAST {
    std::string name;
    int return_type;
    std::vector<std::pair<std::string, int>> params;
    std::unordered_map<std::string, ZulValue> &arg_value_map;

    FuncProtoAST(std::string name, int return_type, std::vector<std::pair<std::string, int>> params,
                 std::unordered_map<std::string, ZulValue> &arg_value_map) :
            name(std::move(name)), return_type(return_type), params(std::move(params)), arg_value_map(arg_value_map) {}

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
            arg.setName(params[i].first);
            arg_value_map.emplace(params[i].first, ZulValue(&arg, params[i].second));
            i++;
        }
        return llvm_func;
    }
};

struct FuncAST {
    FuncProtoAST *proto;
    std::vector<std::unique_ptr<AST>> body;

    FuncAST(FuncProtoAST *proto, std::vector<std::unique_ptr<AST>> body) : proto(proto),
                                                                           body(std::move(body)) {}

    llvm::Function *code_gen(ZulContext &zulctx) {
        auto llvm_func = proto->code_gen(zulctx);
        auto bb = llvm::BasicBlock::Create(zulctx.context, "entry", llvm_func);
        zulctx.builder.SetInsertPoint(bb);

        for (auto &ast: body)
            ast->code_gen(zulctx);

        llvm::verifyFunction(*llvm_func);

        return llvm_func;
    }
};

struct ImmIntAST : public AST {
    long long val;

    explicit ImmIntAST(long long val) : val(val) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        return {llvm::ConstantInt::get(zulctx.context, llvm::APInt(64, val, true)), 1};
    }
};

struct ImmRealAST : public AST {
    double val;

    explicit ImmRealAST(double val) : val(val) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        return {llvm::ConstantFP::get(zulctx.context, llvm::APFloat(val)), 2};
    }
};

struct VariableAST : public AST {
    Capture<std::string> name;

    explicit VariableAST(Capture<std::string> name) : name(std::move(name)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        std::pair<llvm::Value *, int> value;
        if (zulctx.arg_value_map.contains(name.value)) {
            return zulctx.arg_value_map[name.value];
        }
        if (zulctx.local_var_map.contains(name.value)) {
            value = zulctx.local_var_map[name.value];
        } else if (zulctx.global_var_map.contains(name.value)) {
            value = zulctx.global_var_map[name.value];
        } else {
            System::logger.log_error(name.loc, name.word_size, {"\"", name.value, "\" 는 존재하지 않는 변수입니다"});
            return {nullptr, -1};
        }
        auto load = zulctx.builder.CreateLoad(get_llvm_type(zulctx.context, value.second), value.first);
        return {load, value.second};
    }
};

struct VariableDeclAST : public AST {
    Capture<std::string> name;
    int type = -1;
    std::unique_ptr<AST> body = nullptr;

    LocalVarMap &local_var_map;

    VariableDeclAST(Capture<std::string> name, int type, LocalVarMap &local_var_map) :
            name(std::move(name)), type(type), local_var_map(local_var_map) {}

    VariableDeclAST(Capture<std::string> name, std::unique_ptr<AST> body, LocalVarMap &local_var_map) :
            name(std::move(name)), body(std::move(body)), local_var_map(local_var_map) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        llvm::Value *init_val;
        if (body) {
            auto result = body->code_gen(zulctx);
            if (result.value == nullptr)
                return {nullptr, -1};
            init_val = result.value;
            type = result.type_id;
        }
        llvm::AllocaInst *alloca;
        if (local_var_map.contains(name.value)) {
            if (!body) {
                System::logger.log_error(name.loc, name.word_size, "변수가 재정의되었습니다");
                return {nullptr, -1};
            }
            auto origin = local_var_map[name.value];
            if (origin.second != type) {
                System::logger.log_error(name.loc, name.word_size,
                                         {"타입이 일치하지 않습니다. \"", name.value, "\" 변수의 타입은 \"", std::to_string(origin.second),
                                          "\" 이고, 대입 연산식의 타입은 \"", std::to_string(type), "\" 입니다"});
                return {nullptr, -1};
            }
            alloca = origin.first;
        } else {
            auto func = zulctx.builder.GetInsertBlock()->getParent();
            llvm::IRBuilder<> temp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
            alloca = temp_builder.CreateAlloca(get_llvm_type(zulctx.context, type), nullptr, name.value);
        }
        if (body) {
            zulctx.builder.CreateStore(init_val, alloca);
        }
        local_var_map.emplace(name.value, std::make_pair(alloca, type));
        return {alloca, type};
    }
};

struct BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Capture<Token> op;

    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Capture<Token> op) : left(std::move(left)),
                                                                                         right(std::move(right)),
                                                                                         op(std::move(op)) {}

    static void create_cast(ZulContext &zulctx, ZulValue &target, int type_id) {
        if (type_id == 1) {
            target.value = zulctx.builder.CreateIntCast(target.value, llvm::Type::getInt64Ty(zulctx.context), true);
        } else if (type_id == 2) {
            target.value = zulctx.builder.CreateSIToFP(target.value, llvm::Type::getDoubleTy(zulctx.context));
        }
    }

    llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs) {
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
            case tok_and:
            case tok_or:
            case tok_not:
            case tok_bitand:
            case tok_bitor:
            case tok_bitnot:
            case tok_bitxor:
            case tok_lshift:
            case tok_rshift:
            case tok_assn:
            case tok_mul_assn:
            case tok_div_assn:
            case tok_mod_assn:
            case tok_add_assn:
            case tok_sub_assn:
            case tok_lshift_assn:
            case tok_rshift_assn:
            case tok_and_assn:
            case tok_or_assn:
            case tok_xor_assn:
            case tok_eq:
            case tok_ineq:
            case tok_gt:
            case tok_gteq:
            case tok_lt:
            case tok_lteq:
            default:
                System::logger.log_error(op.loc, op.word_size, "존재하지 않는 연산자입니다");
                return nullptr;
        }
    }

    llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs) {
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
            default:
                System::logger.log_error(op.loc, op.word_size, "존재하지 않는 연산자입니다");
                return nullptr;
        }
    }

    ZulValue code_gen(ZulContext &zulctx) override {
        auto lhs = left->code_gen(zulctx);
        auto rhs = right->code_gen(zulctx);
        if (!lhs.value || !rhs.value)
            return {nullptr, -1};
        if (lhs.type_id > 2 || rhs.type_id > 2) { //연산자 오버로딩 지원 하게되면 변경
            return {nullptr, -1};
        }
        int calc_type = lhs.type_id;
        if (lhs.type_id > rhs.type_id) {
            calc_type = lhs.type_id;
            create_cast(zulctx, rhs, lhs.type_id);
        } else if (lhs.type_id < rhs.type_id) {
            calc_type = rhs.type_id;
            create_cast(zulctx, lhs, rhs.type_id);
        }
        llvm::Value *ret;
        if (calc_type < 2) {
            ret = create_int_operation(zulctx, lhs.value, rhs.value);
        } else {
            ret = create_float_operation(zulctx, lhs.value, rhs.value);
        }
        return {ret, calc_type};
    }
};

struct UnaryOpAST : public AST {
    std::unique_ptr<AST> ast;
    Token op;

    UnaryOpAST(std::unique_ptr<AST> ast, Token op) : ast(std::move(ast)), op(op) {}
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
            if (arg.type_id != proto->params[i].second) { //arg와 param의 타입이 맞지 않으면
                System::logger.log_error(args[i].loc, args[i].word_size, {
                        "타입이 일치하지 않습니다.", " 인자의 타입은 \"", std::to_string(arg.type_id), "\" 이고, 매개변수의 타입은 \"",
                        std::to_string(proto->params[i].second), "\" 입니다"});
                has_error = true;
            }
            arg_values.push_back(arg.value);
        }
        if (has_error)
            return {nullptr, -1};
        return {zulctx.builder.CreateCall(target_func, arg_values), proto->return_type};
    }
};

#endif //ZULLANG_AST_H
