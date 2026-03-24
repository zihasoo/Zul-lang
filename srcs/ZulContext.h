
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULC_ZULCONTEXT_H
#define ZULC_ZULCONTEXT_H

#include <memory>
#include <utility>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "System.h"
#include "Utility.h"
#include "Lexer.h"

struct ExprAST;

enum eTypeID {
    ID_BOOL,
    ID_CHAR,
    ID_INT,
    ID_FLOAT,
    ID_MAX,
    ID_INTERRUPT = -10
};

std::string id_to_name(int type_id);

struct ZulValue {
    llvm::Value* value;
    int id;

    ZulValue() = default;

    ZulValue(llvm::Value *value, int id) : value(value), id(id) {}

    ZulValue(std::pair<llvm::AllocaInst *, int> pair) : value(pair.first), id(pair.second) {}

    ZulValue(std::pair<llvm::GlobalVariable *, int> pair) : value(pair.first), id(pair.second) {}
};

extern const ZulValue nullzul;

using ASTPtr = std::unique_ptr<ExprAST>;
using CondBodyPair = std::pair<ASTPtr, std::vector<ASTPtr>>;

//현재 진행 상태에서 파싱과 코드 생성의 모든 정보를 담는 콘텍스트 객체
struct ZulContext {
    std::unique_ptr<llvm::LLVMContext> context{new llvm::LLVMContext{}};
    std::unique_ptr<llvm::Module> module{new llvm::Module{System::source_base_name, *context}};
    llvm::IRBuilder<> builder{*context};
    std::map<std::string, std::pair<llvm::GlobalVariable *, int>> global_var_map;
    std::map<std::string, std::pair<llvm::AllocaInst *, int>> local_var_map;
    std::stack<std::vector<std::string>> scope_stack;
    std::stack<llvm::BasicBlock *> loop_update_stack;
    std::stack<llvm::BasicBlock *> loop_end_stack;
    llvm::BasicBlock *return_block{};
    llvm::AllocaInst *return_var{};
    int ret_count = 0;
    bool in_loop = false;

    bool is_declared(const std::string &var_name) const;

    void remove_scope_vars();

    llvm::Constant* zero_of(int type_id) const;

    llvm::Type* id_to_type(int type_id) const;

    bool create_cast(ZulValue &source, int dest_type_id);

    llvm::Value *create_int_operation(llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

    llvm::Value *create_float_operation(llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

    bool to_boolean_expr(ZulValue &expr);
};


#endif //ZULC_ZULCONTEXT_H
