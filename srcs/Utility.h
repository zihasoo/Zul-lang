#ifndef ZULLANG_UTILITY_H
#define ZULLANG_UTILITY_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

#include <string>
#include <string_view>

llvm::Constant *get_llvm_constant(llvm::Type *llvm_type, int type_num);

llvm::Type *get_llvm_type(llvm::LLVMContext &context, int type_num);

int get_byte_count(int c);

bool iskor(int c);

bool isnum(int c);

bool iskornum(int c);

#endif //ZULLANG_UTILITY_H
