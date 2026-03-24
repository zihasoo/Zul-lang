
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <memory>
#include <utility>
#include <vector>
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
#include "ZulContext.h"

struct ExprAST {
    virtual ~ExprAST() = default;

    virtual ZulValue code_gen(ZulContext &zul_context) = 0;

    virtual bool is_const();

    virtual bool is_lvalue();

    virtual int get_typeid(ZulContext &zul_context);
};

struct LValueAST : ExprAST {
    virtual ZulValue get_origin_value(ZulContext &zul_context) = 0;

    bool is_lvalue() override;
};

struct FuncProtoAST {
    std::string name;
    int return_type = -1;
    bool has_body;
    bool is_var_arg;
    std::vector<std::pair<std::string, int>> params;

    FuncProtoAST() = default;

    FuncProtoAST(std::string name, int return_type, std::vector<std::pair<std::string, int>> params,
                 bool has_body, bool is_var_arg);

    llvm::Function *code_gen(ZulContext &zul_context) const;
};

struct FuncRetAST : public ExprAST {
    ASTPtr body;
    Capture<int> return_type;

    FuncRetAST(ASTPtr body, Capture<int> return_type);

    ZulValue code_gen(ZulContext &zul_context) override;

    int get_typeid(ZulContext &zul_context) override;
};

struct IfAST : public ExprAST {
    CondBodyPair if_pair;
    std::vector<CondBodyPair> elif_pair_list;
    std::vector<ASTPtr> else_body;

    IfAST(CondBodyPair if_pair, std::vector<CondBodyPair> elif_pair_list, std::vector<ASTPtr> else_body);

    ZulValue code_gen(ZulContext &zul_context) override;
};

struct LoopAST : public ExprAST {
    ASTPtr init_body;
    ASTPtr test_body;
    ASTPtr update_body;
    std::vector<ASTPtr> loop_body;

    LoopAST(ASTPtr init_body, ASTPtr test_body, ASTPtr update_body, std::vector<ASTPtr> loop_body);

    ZulValue code_gen(ZulContext &zul_context) override;
};

struct ContinueAST : public ExprAST {
    ZulValue code_gen(ZulContext &zul_context) override;
};

struct BreakAST : public ExprAST {
    ZulValue code_gen(ZulContext &zul_context) override;
};

struct VariableAST : public LValueAST {
    std::string name;

    explicit VariableAST(std::string name);

    ZulValue get_origin_value(ZulContext &zul_context) override;

    ZulValue code_gen(ZulContext &zul_context) override;

    int get_typeid(ZulContext &zul_context) override;
};

struct VariableDeclAST : public ExprAST {
    Capture<std::string> name;
    int type;
    ASTPtr body;

    VariableDeclAST(Capture<std::string> name, ZulContext &zul_context, int type, ASTPtr body = nullptr);

    VariableDeclAST(Capture<std::string> name, ZulContext &zul_context, ASTPtr body);

    void register_var(ZulContext &zul_context);

    ZulValue code_gen(ZulContext &zul_context) override;
};

struct VariableAssnAST : public ExprAST {
    std::unique_ptr<LValueAST> target;
    Capture<Token> op;
    ASTPtr body;

    VariableAssnAST(std::unique_ptr<LValueAST> target, Capture<Token> op, ASTPtr body);

    ZulValue code_gen(ZulContext &zul_context) override;
};

struct BinOpAST : public ExprAST {
    ASTPtr left, right;
    Capture<Token> op;

    BinOpAST(ASTPtr left, ASTPtr right, Capture<Token> op);

    ZulValue short_circuit_code_gen(ZulContext &zul_context) const;

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};

struct UnaryOpAST : public ExprAST {
    ASTPtr body;
    Capture<Token> op;

    UnaryOpAST(ASTPtr body, Capture<Token> op);

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};

struct SubscriptAST : public LValueAST {
    std::unique_ptr<VariableAST> target;
    Capture<ASTPtr> index;

    SubscriptAST(std::unique_ptr<VariableAST> target, Capture<ASTPtr> index);

    ZulValue get_origin_value(ZulContext &zul_context) override;

    ZulValue code_gen(ZulContext &zul_context) override;

    int get_typeid(ZulContext &zul_context) override;
};

struct FuncCallAST : public ExprAST {
    FuncProtoAST &proto;
    std::vector<Capture<ASTPtr>> args;

    static std::unordered_map<int, std::string_view> format_str_map;

    FuncCallAST(FuncProtoAST &proto, std::vector<Capture<ASTPtr>> args);

    std::string_view get_format_str(int type_id);

    ZulValue handle_std_in(ZulContext &zul_context);

    ZulValue handle_std_out(ZulContext &zul_context);

    ZulValue code_gen(ZulContext &zul_context) override;

    int get_typeid(ZulContext &zul_context) override;
};

struct ImmBoolAST : public ExprAST {
    bool val;

    explicit ImmBoolAST(bool val);

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};

struct ImmCharAST : public ExprAST {
    char val;

    explicit ImmCharAST(char val);

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};


struct ImmIntAST : public ExprAST {
    long long val;

    explicit ImmIntAST(long long val);

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};

struct ImmRealAST : public ExprAST {
    double val;

    explicit ImmRealAST(double val);

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};

struct ImmStrAST : public ExprAST {
    std::string val;

    explicit ImmStrAST(std::string val);

    ZulValue code_gen(ZulContext &zul_context) override;

    bool is_const() override;

    int get_typeid(ZulContext &zul_context) override;
};

#endif //ZULLANG_AST_H
