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

struct ZulContext {
    llvm::LLVMContext context;
    llvm::Module module{"zul", context};
    llvm::IRBuilder<> builder{context};
    std::map<std::string, std::pair<llvm::GlobalVariable *, int>> global_var_map;
    LocalVarMap local_var_map;

    ZulContext() {
        local_var_map.reserve(30);
    }
};

struct ZulValue {
    llvm::Value *value;
    int type_id;
};

struct ArgCapture {
    std::unique_ptr<AST> arg;
    std::pair<int, int> loc;
    int word_size;

    ArgCapture(std::unique_ptr<AST> arg,
               std::pair<int, int> loc,
               int word_size) : arg(std::move(arg)), loc(std::move(loc)), word_size(word_size) {}
};

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
            arg.setName(params[i].first);
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
    std::string name;

    VariableAST(std::string name) : name(std::move(name)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        std::pair<llvm::Value*, int> value;
        if (zulctx.local_var_map.contains(name)) {
            value = zulctx.local_var_map[name];
        }
        else if (zulctx.global_var_map.contains(name)) {
            value = zulctx.global_var_map[name];
        }
        auto load = zulctx.builder.CreateLoad(get_llvm_type(zulctx.context, value.second), value.first);
        return {load, value.second};
    }
};

struct VariableDeclAST : public AST {
    std::string name;
    int type = -1;
    std::unique_ptr<AST> body = nullptr;

    LocalVarMap *local_var_map;

    VariableDeclAST(std::string name, int type, LocalVarMap *local_var_map) :
            name(std::move(name)), type(type), local_var_map(local_var_map) {}

    VariableDeclAST(std::string name, std::unique_ptr<AST> body, LocalVarMap *local_var_map) :
            name(std::move(name)), body(std::move(body)), local_var_map(local_var_map) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        llvm::Value *init_val;
        auto func = zulctx.builder.GetInsertBlock()->getParent();
        llvm::IRBuilder<> temp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
        if (type == -1) {
            auto result = body->code_gen(zulctx);
            if (result.value == nullptr)
                return {nullptr, -1};
            init_val = result.value;
            type = result.type_id;
        }
        auto alloca = temp_builder.CreateAlloca(get_llvm_type(zulctx.context, type), nullptr, name);
        if (body) {
            zulctx.builder.CreateStore(init_val, alloca);
        }
        local_var_map->emplace(name, std::make_pair(alloca, type));
        return {alloca, type};
    }
};

struct BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Token op;

    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Token op) : left(std::move(left)),
                                                                                right(std::move(right)), op(op) {}

    static void create_cast_inst(ZulContext &zulctx, ZulValue &target, int type_id) {
        if (type_id == 1) {
            target.value = zulctx.builder.CreateIntCast(target.value, llvm::Type::getInt64Ty(zulctx.context), true);
        } else if (type_id == 2) {
            target.value = zulctx.builder.CreateSIToFP(target.value, llvm::Type::getDoubleTy(zulctx.context));
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
            create_cast_inst(zulctx, rhs, lhs.type_id);
        } else if (lhs.type_id < rhs.type_id) {
            calc_type = rhs.type_id;
            create_cast_inst(zulctx, lhs, rhs.type_id);
        }
        llvm::Value *ret;
        switch (op) {
            case tok_add:
                ret = zulctx.builder.CreateAdd(lhs.value, rhs.value);
                break;
            case tok_sub:
                ret = zulctx.builder.CreateSub(lhs.value, rhs.value);
                break;
            case tok_mul:
                break;
            case tok_div:
                break;
            case tok_mod:
                break;
            case tok_and:
                break;
            case tok_or:
                break;
            case tok_not:
                break;
            case tok_bitand:
                break;
            case tok_bitor:
                break;
            case tok_bitnot:
                break;
            case tok_bitxor:
                break;
            case tok_lshift:
                break;
            case tok_rshift:
                break;
            case tok_assn:
                break;
            case tok_mul_assn:
                break;
            case tok_div_assn:
                break;
            case tok_mod_assn:
                break;
            case tok_add_assn:
                break;
            case tok_sub_assn:
                break;
            case tok_lshift_assn:
                break;
            case tok_rshift_assn:
                break;
            case tok_and_assn:
                break;
            case tok_or_assn:
                break;
            case tok_xor_assn:
                break;
            case tok_eq:
                break;
            case tok_ineq:
                break;
            case tok_gt:
                break;
            case tok_gteq:
                break;
            case tok_lt:
                break;
            case tok_lteq:
                break;
            default:
                return {nullptr, -1};
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
    std::vector<ArgCapture> args;

    FuncCallAST(FuncProtoAST *proto,
                std::vector<ArgCapture> args)
            : proto(proto), args(std::move(args)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        auto target_func = zulctx.module.getFunction(proto->name);
        std::vector<llvm::Value *> arg_values;
        bool has_error = false;
        arg_values.reserve(args.size());
        for (int i = 0; i < args.size(); i++) {
            auto arg = args[i].arg->code_gen(zulctx);
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
