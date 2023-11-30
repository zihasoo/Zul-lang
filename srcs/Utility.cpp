#include <sstream>

#include <llvm/IR/Constants.h>

#include "Utility.h"

std::map<int, std::string> type_name_map = {
        {-1, "없음"},
        {0,  "논리"},
        {1,  "글자"},
        {2,  "수"},
        {3,  "소수"},
        {4,  "글"}
};

llvm::Constant *get_const_zero(llvm::LLVMContext &context, int type_num) {
    return get_const_zero(get_llvm_type(context, type_num), type_num);
}

llvm::Constant *get_const_zero(llvm::Type *llvm_type, int type_num) {
    switch (type_num) {
        case BOOL_TYPEID:
        case 1:
        case 2:
            return llvm::ConstantInt::get(llvm_type, 0, true);
        case FLOAT_TYPEID:
            return llvm::ConstantFP::get(llvm_type, 0);
        case 4:
            return llvm::ConstantPointerNull::get(static_cast<llvm::PointerType *>(llvm_type));
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
        case 4:
            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context));
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

std::string token_to_string(Token token) {
    switch (token) {
        case tok_hi:
            return "tok_hi";
        case tok_go:
            return "tok_go";
        case tok_ij:
            return "tok_ij";
        case tok_no:
            return "tok_no";
        case tok_nope:
            return "tok_nope";
        case tok_gg:
            return "tok_gg";
        case tok_sg:
            return "tok_sg";
        case tok_tt:
            return "tok_tt";
        case tok_identifier:
            return "tok_identifier";
        case tok_int:
            return "tok_int";
        case tok_real:
            return "tok_real";
        case tok_string:
            return "tok_string";
        case tok_eng:
            return "tok_eng";
        case tok_indent:
            return "tok_indent";
        case tok_newline:
            return "tok_newline";
        case tok_comma:
            return "tok_comma";
        case tok_colon:
            return "tok_colon";
        case tok_semicolon:
            return "tok_semicolon";
        case tok_lpar:
            return "tok_lpar";
        case tok_rpar:
            return "tok_rpar";
        case tok_rsqbrk:
            return "tok_rsqbrk";
        case tok_lsqbrk:
            return "tok_lsqbrk";
        case tok_rbrk:
            return "tok_rbrk";
        case tok_lbrk:
            return "tok_lbrk";
        case tok_dot:
            return "tok_dot";
        case tok_dquotes:
            return "tok_dquotes";
        case tok_squotes:
            return "tok_squotes";
        case tok_anno:
            return "tok_anno";
        case tok_add:
            return "tok_add";
        case tok_sub:
            return "tok_sub";
        case tok_mul:
            return "tok_mul";
        case tok_div:
            return "tok_div";
        case tok_mod:
            return "tok_mod";
        case tok_and:
            return "tok_and";
        case tok_or:
            return "tok_or";
        case tok_not:
            return "tok_not";
        case tok_bitand:
            return "tok_bitand";
        case tok_bitor:
            return "tok_bitor";
        case tok_bitnot:
            return "tok_bitnot";
        case tok_bitxor:
            return "tok_bitxor";
        case tok_lshift:
            return "tok_lshift";
        case tok_rshift:
            return "tok_rshift";
        case tok_assn:
            return "tok_assn";
        case tok_mul_assn:
            return "tok_mul_assn";
        case tok_div_assn:
            return "tok_div_assn";
        case tok_mod_assn:
            return "tok_mod_assn";
        case tok_add_assn:
            return "tok_add_assn";
        case tok_sub_assn:
            return "tok_sub_assn";
        case tok_lshift_assn:
            return "tok_lshift_assn";
        case tok_rshift_assn:
            return "tok_rshift_assn";
        case tok_and_assn:
            return "tok_and_assn";
        case tok_or_assn:
            return "tok_or_assn";
        case tok_xor_assn:
            return "tok_xor_assn";
        case tok_eq:
            return "tok_eq";
        case tok_ineq:
            return "tok_ineq";
        case tok_gt:
            return "tok_gt";
        case tok_gteq:
            return "tok_gteq";
        case tok_lt:
            return "tok_lt";
        case tok_lteq:
            return "tok_lteq";
        case tok_eof:
            return "tok_eof";
        default:
            return "tok_undefined";
    }
}