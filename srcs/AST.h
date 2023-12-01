#ifndef ZULLANG_AST_H
#define ZULLANG_AST_H

#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <list>
#include <stack>
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

struct ExprAST;

using GlobalVarMap = std::map<std::string, std::pair<llvm::GlobalVariable *, int>>;
using LocalVarMap = std::unordered_map<std::string, std::pair<llvm::AllocaInst *, int>>;
using ZulValue = std::pair<llvm::Value *, int>;
using ASTPtr = std::unique_ptr<ExprAST>;

extern ZulValue nullzul;

struct ZulContext {
    std::unique_ptr<llvm::LLVMContext> context{new llvm::LLVMContext{}};
    std::unique_ptr<llvm::Module> module{new llvm::Module{"zul", *context}};
    llvm::IRBuilder<> builder{*context};
    GlobalVarMap global_var_map;
    LocalVarMap local_var_map;
    std::stack<std::list<std::string>> scope_stack;
    std::stack<llvm::BasicBlock*> loop_update_stack;
    std::stack<llvm::BasicBlock*> loop_end_stack;
    llvm::BasicBlock *return_block;
    llvm::AllocaInst *return_var;
    int ret_count = 0;

    ZulContext();
};

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id);

llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

bool is_cmp(Token op);

struct ExprAST {
    virtual ~ExprAST() = default;

    virtual ZulValue code_gen(ZulContext &zulctx) = 0;
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

    llvm::Function *code_gen(ZulContext &zulctx);
};

struct FuncRetAST : public ExprAST {
    Capture<int> return_type;
    ASTPtr body;

    FuncRetAST(ASTPtr body, Capture<int> return_type);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct IfAST : public ExprAST {
    ASTPtr cond;
    ASTPtr body;

//    std::vector<ASTPtr> elif_ast;
//    ASTPtr else_ast;
    IfAST(ASTPtr cond, ASTPtr body) :
            cond(std::move(cond)), body(std::move(body)) {}

    ZulValue code_gen(ZulContext &zulctx) override {
        return nullzul;
    }
};

struct ElifAST : public ExprAST {
    ASTPtr cond;
    ASTPtr body;
};

struct ElseAST : public ExprAST {

};

struct LoopAST : public ExprAST {
    ASTPtr init_body;
    ASTPtr test_body;
    ASTPtr update_body;
    std::vector<ASTPtr> loop_body;

    LoopAST(ASTPtr init_body, ASTPtr test_body, ASTPtr update_body, std::vector<ASTPtr> loop_body);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ContinueAST : public ExprAST {
    ZulValue code_gen(ZulContext &zulctx) override;
};

struct BreakAST : public ExprAST {
    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmBoolAST : public ExprAST {
    bool val;

    explicit ImmBoolAST(bool val);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmCharAST : public ExprAST {
    char val;

    explicit ImmCharAST(char val);

    ZulValue code_gen(ZulContext &zulctx) override;
};


struct ImmIntAST : public ExprAST {
    long long val;

    explicit ImmIntAST(long long val);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmRealAST : public ExprAST {
    double val;

    explicit ImmRealAST(double val);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmStrAST : public ExprAST {
    std::string val;

    explicit ImmStrAST(std::string val);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct VariableAST : public ExprAST {
    std::string name;

    explicit VariableAST(std::string name);

    std::pair<llvm::Value *, int> get_origin_value(ZulContext &zulctx) const;

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct VariableDeclAST : public ExprAST {
    Capture<std::string> name;
    int type = -1;
    ASTPtr body = nullptr;

    VariableDeclAST(Capture<std::string> name, int type);

    VariableDeclAST(Capture<std::string> name, ASTPtr body);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct VariableAssnAST : public ExprAST {
    std::unique_ptr<VariableAST> target;
    Capture<Token> op;
    ASTPtr body;

    VariableAssnAST(std::unique_ptr<VariableAST> target, Capture<Token> op, ASTPtr body);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct BinOpAST : public ExprAST {
    ASTPtr left, right;
    Capture<Token> op;

    BinOpAST(ASTPtr left, ASTPtr right, Capture<Token> op);


    ZulValue code_gen(ZulContext &zulctx) override;
};

struct UnaryOpAST : public ExprAST {
    ASTPtr body;
    Capture<Token> op;

    UnaryOpAST(ASTPtr body, Capture<Token> op);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct FuncCallAST : public ExprAST {
    FuncProtoAST &proto;
    std::vector<Capture<ASTPtr>> args;

    FuncCallAST(FuncProtoAST &proto, std::vector<Capture<ASTPtr>> args);

    ZulValue code_gen(ZulContext &zulctx) override;
};

#endif //ZULLANG_AST_H
