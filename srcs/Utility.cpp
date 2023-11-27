#include <llvm/IR/Constants.h>
#include "Utility.h"

llvm::Constant *get_const_zero(llvm::Type *llvm_type, int type_num) {
    switch (type_num) {
        case BOOL_TYPEID ... 2:
            return llvm::ConstantInt::get(llvm_type, 0, true);
        case FLOAT_TYPEID:
            return llvm::ConstantFP::get(llvm_type, 0);
        default:
            return nullptr;
    }
}

llvm::Type *get_llvm_type(llvm::LLVMContext &context, int type_num) {
    switch (type_num) {
        case BOOL_TYPEID:
            return llvm::Type::getInt1Ty(context);
        case 1:
            return llvm::Type::getInt8Ty(context);
        case 2:
            return llvm::Type::getInt64Ty(context);
        case FLOAT_TYPEID:
            return llvm::Type::getDoubleTy(context);
        default:
            return llvm::Type::getVoidTy(context);
    }
}

int get_byte_count(int c) {
    if ((c & 0x80) == 0)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return -1;
}

bool iskor(int c) {
    return (0x3131 <= c && c <= 0x318E) || (0xAC00 <= c && c <= 0xD7FF);
}

bool isnum(int c) {
    return '0' <= c && c <= '9';
}

bool iskornum(int c) {
    return iskor(c) || isnum(c);
}
