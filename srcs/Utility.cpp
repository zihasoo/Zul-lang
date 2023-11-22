#include "Utility.h"

llvm::Type *get_llvm_type(llvm::LLVMContext *context, int type_num) {
    if (type_num == 0) {
        return llvm::Type::getInt64Ty(*context);
    } else if (type_num == 1) {
        return llvm::Type::getDoubleTy(*context);
    } else {
        return llvm::Type::getVoidTy(*context);
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
    return (0x3131 <= c && c <= 0x318E) || (0xAC00 <= c && c <= 0xD7FF) || c == '_';
}

bool isnum(int c) {
    return '0' <= c && c <= '9';
}

bool iskornum(int c) {
    return iskor(c) || isnum(c);
}
