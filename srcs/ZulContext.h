
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULC_ZULCONTEXT_H
#define ZULC_ZULCONTEXT_H

#include <memory>
#include <utility>
#include <map>
#include <stack>
#include <vector>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "System.h"

struct ExprAST;

using ZulValue = std::pair<llvm::Value *, int>;
using ASTPtr = std::unique_ptr<ExprAST>;
using CondBodyPair = std::pair<ASTPtr, std::vector<ASTPtr>>;

//현재 진행 상태에서 파싱과 코드 생성의 모든 정보를 담는 콘텍스트 객체
struct ZulContext {
    std::unique_ptr<llvm::LLVMContext> context{new llvm::LLVMContext{}};
    std::unique_ptr<llvm::Module> module{new llvm::Module{System::source_base_name, *context}};
    llvm::IRBuilder<> builder{*context};
    std::map<std::string, std::pair<llvm::GlobalVariable *, int>> global_var_map;
    std::unordered_map<std::string, std::pair<llvm::AllocaInst *, int>> local_var_map;
    std::stack<std::vector<std::string>> scope_stack;
    std::stack<llvm::BasicBlock *> loop_update_stack;
    std::stack<llvm::BasicBlock *> loop_end_stack;
    llvm::BasicBlock *return_block{};
    llvm::AllocaInst *return_var{};
    int ret_count = 0;
    bool in_loop = false;

    ZulContext();

    bool var_exist(const std::string &name);

    void remove_scope_vars();
};


#endif //ZULC_ZULCONTEXT_H
