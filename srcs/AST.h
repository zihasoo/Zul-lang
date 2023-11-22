#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <memory>
#include <utility>
#include <format>
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

struct AST {
public:
    virtual ~AST() = default;

    virtual llvm::Value *code_gen(llvm::LLVMContext *context) {
        return nullptr;
    };
};

struct ImmRealAST : public AST {
    double val;

    explicit ImmRealAST(double val) : val(val) {}

    llvm::Value *code_gen(llvm::LLVMContext *context) override {
        return llvm::ConstantFP::get(*context, llvm::APFloat(val));
    }
};

struct ImmIntAST : public AST {
    long long val;

    explicit ImmIntAST(long long val) : val(val) {}

    llvm::Value *code_gen(llvm::LLVMContext *context) override {
        return llvm::ConstantInt::get(*context, llvm::APInt(64, val, true));
    }
};

struct ImmStringAST : public AST {
    std::string val;

    explicit ImmStringAST(std::string val) : val(std::move(val)) {}

    llvm::Value *code_gen(llvm::LLVMContext *context) override {
        return nullptr;
    }
};

struct VariableAST : public AST {
    std::string name;
    int type;

    VariableAST(std::string name, int type) : name(std::move(name)), type(type) {}

    llvm::Value *code_gen(llvm::LLVMContext *context) override {
        return nullptr;
    }
};

struct BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Token op;

    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Token op) : left(std::move(left)),
                                                                                right(std::move(right)), op(op) {}

    llvm::Value *code_gen(llvm::LLVMContext *context) override {
        auto l = left->code_gen(context);
        auto r = right->code_gen(context);
        if (!l || !r)
            return nullptr;
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
};

struct FuncAST {
    std::string name;
    int return_type = 0;
    std::vector<VariableAST> params;
    std::unique_ptr<AST> body;

    FuncAST(std::string name, int return_type, std::vector<VariableAST> params, std::unique_ptr<AST> body)
            : name(std::move(name)), return_type(return_type), params(std::move(params)), body(std::move(body)) {}

    llvm::Function *code_gen(llvm::LLVMContext *context, llvm::Module *module, llvm::IRBuilder<> *builder) {
        std::vector<llvm::Type *> param_types;
        param_types.reserve(params.size());
        for (auto &p: params) {
            param_types.emplace_back(get_llvm_type(context, p.type));
        }
        auto func_type = llvm::FunctionType::get(get_llvm_type(context, return_type), param_types, false);
        auto llvm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, name, *module);
        int i = 0;
        for (auto &arg: llvm_func->args()) {
            arg.setName(params[i].name);
        }

        auto bb = llvm::BasicBlock::Create(*context, "entry", llvm_func);
        builder->SetInsertPoint(bb);

        llvm::verifyFunction(*llvm_func);

        return llvm_func;
    }
};

#endif //ZULLANG_AST_H
