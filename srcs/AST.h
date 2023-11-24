#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <memory>
#include <utility>
#include <vector>

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

#include "Lexer.h"

struct IRTools {
    llvm::LLVMContext context;
    llvm::Module module{ "zul", context };
    llvm::IRBuilder<> builder{context};
};

struct AST {
public:
    virtual ~AST() = default;

    virtual llvm::Value *code_gen(IRTools &ir_tools) = 0;
};

struct ImmRealAST : public AST {
    double val;

    explicit ImmRealAST(double val) : val(val) {}

    llvm::Value *code_gen(IRTools &ir_tools) override {
        return llvm::ConstantFP::get(ir_tools.context, llvm::APFloat(val));
    }
};

struct ImmIntAST : public AST {
    long long val;

    explicit ImmIntAST(long long val) : val(val) {}

    llvm::Value *code_gen(IRTools &ir_tools) override {
        return llvm::ConstantInt::get(ir_tools.context, llvm::APInt(64, val, true));
    }
};

struct ImmStringAST : public AST {
    std::string val;

    explicit ImmStringAST(std::string val) : val(std::move(val)) {}

    llvm::Value *code_gen(IRTools &ir_tools) override {
        return nullptr;
    }
};

struct VariableAST : public AST {
    std::string name;
    int type;

    VariableAST(std::string name, int type) : name(std::move(name)), type(type) {}

    llvm::Value *code_gen(IRTools &ir_tools) override {
        return nullptr;
    }
};

struct BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Token op;

    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Token op) : left(std::move(left)),
                                                                                right(std::move(right)), op(op) {}

    llvm::Value *code_gen(IRTools &ir_tools) override {
        auto l = left->code_gen(ir_tools);
        auto r = right->code_gen(ir_tools);
        if (!l || !r)
            return nullptr;
        switch (op) {
            case tok_add:
                break;
            case tok_sub:
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
                return nullptr;
        }
        return nullptr;
    }
};

struct UnaryOpAST : public AST {
    std::unique_ptr<AST> ast;
    Token op;

    UnaryOpAST(std::unique_ptr<AST> ast, Token op) : ast(std::move(ast)), op(op) {}
};

struct FuncCallAST : public AST {
    std::string name;
    std::vector<std::unique_ptr<AST>> args;

    FuncCallAST(std::string name, std::vector<std::unique_ptr<AST>> args)
            : name(std::move(name)), args(std::move(args)) {}

    llvm::Value *code_gen(IRTools &ir_tools) override {
        auto target_func = ir_tools.module.getFunction(name);
        std::vector<llvm::Value *> arg_values;
        arg_values.reserve(args.size());
        for (const auto & arg : args) {
            arg_values.push_back(arg->code_gen(ir_tools));
            if (!arg_values.back())
                return nullptr;
        }
        return ir_tools.builder.CreateCall(target_func, arg_values, "calltmp");
    }
};

struct FuncProtoAST {
    std::string name;
    int return_type;
    std::vector<VariableAST> params;

    FuncProtoAST(std::string name, int return_type, std::vector<VariableAST> params) :
            name(std::move(name)), return_type(return_type), params(std::move(params)) {}

    llvm::Function *code_gen(IRTools &ir_tools) {
        std::vector<llvm::Type *> param_types;
        param_types.reserve(params.size());
        for (auto &param: params) {
            param_types.emplace_back(get_llvm_type(ir_tools.context, param.type));
        }
        auto func_type = llvm::FunctionType::get(get_llvm_type(ir_tools.context, return_type), param_types, false);
        auto llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, ir_tools.module);
        int i = 0;
        for (auto &arg: llvm_func->args()) {
            arg.setName(params[i].name);
        }
        return llvm_func;
    }
};

struct FuncAST {
    FuncProtoAST* proto;
    std::vector<std::unique_ptr<AST>> body;

    FuncAST(FuncProtoAST* proto, std::vector<std::unique_ptr<AST>> body) : proto(proto),
                                                                              body(std::move(body)) {}

    llvm::Function *code_gen(IRTools &ir_tools) {
        auto llvm_func = proto->code_gen(ir_tools);
        auto bb = llvm::BasicBlock::Create(ir_tools.context, "entry", llvm_func);
        ir_tools.builder.SetInsertPoint(bb);

        for (auto& ast : body)
            ast->code_gen(ir_tools);

        llvm::verifyFunction(*llvm_func);

        return llvm_func;
    }
};

#endif //ZULLANG_AST_H
