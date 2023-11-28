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

    operator std::pair<llvm::Value *, int>();
};

struct ZulContext {
    std::unique_ptr<llvm::LLVMContext> context{new llvm::LLVMContext{}};
    std::unique_ptr<llvm::Module> module{new llvm::Module{"zul", *context}};
    llvm::IRBuilder<> builder{*context};
    LocalVarMap local_var_map;
    GlobalVarMap global_var_map;

    ZulContext();
};

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id);

llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

static bool is_cmp(Token op);

struct AST {
public:
    virtual ~AST() = default;

    virtual ZulValue code_gen(ZulContext &zulctx) = 0;
};

struct FuncProtoAST {
    std::string name;
    int return_type = -1;
    bool has_body = true;
    std::vector<std::pair<std::string, int>> params;

    FuncProtoAST() = default;

    FuncProtoAST(std::string name, int return_type, std::vector<std::pair<std::string, int>> params,
                 bool has_body = true);

    llvm::Function *code_gen(ZulContext &zulctx);
};

struct FuncRetAST : public AST {
    Capture<int> return_type;
    std::unique_ptr<AST> body;

    FuncRetAST(std::unique_ptr<AST> body, Capture<int> return_type);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmIntAST : public AST {
    long long val;

    explicit ImmIntAST(long long val);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmRealAST : public AST {
    double val;

    explicit ImmRealAST(double val);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct ImmStrAST : public AST {
    std::string val;

    explicit ImmStrAST(std::string val);

    ZulValue code_gen(ZulContext &zulctx) override;
};


struct VariableAST : public AST {
    Capture<std::string> name;

    explicit VariableAST(Capture<std::string> name);

    std::pair<llvm::Value *, int> get_origin_value(ZulContext &zulctx);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct VariableDeclAST : public AST {
    Capture<std::string> name;
    int type = -1;
    std::unique_ptr<AST> body = nullptr;

    VariableDeclAST(Capture<std::string> name, int type);

    VariableDeclAST(Capture<std::string> name, std::unique_ptr<AST> body);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct VariableAssnAST : public AST {
    std::unique_ptr<VariableAST> target;
    Capture<Token> op;
    std::unique_ptr<AST> body;

    VariableAssnAST(std::unique_ptr<VariableAST> target, Capture<Token> op, std::unique_ptr<AST> body);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct BinOpAST : public AST {
    std::unique_ptr<AST> left, right;
    Capture<Token> op;

    BinOpAST(std::unique_ptr<AST> left, std::unique_ptr<AST> right, Capture<Token> op);


    ZulValue code_gen(ZulContext &zulctx) override;
};

struct UnaryOpAST : public AST {
    std::unique_ptr<AST> body;
    Capture<Token> op;

    UnaryOpAST(std::unique_ptr<AST> body, Capture<Token> op);

    ZulValue code_gen(ZulContext &zulctx) override;
};

struct FuncCallAST : public AST {
    FuncProtoAST &proto;
    std::vector<Capture<std::unique_ptr<AST>>> args;

    FuncCallAST(FuncProtoAST &proto, std::vector<Capture<std::unique_ptr<AST>>> args);

    ZulValue code_gen(ZulContext &zulctx) override;
};

#endif //ZULLANG_AST_H
