#include <sstream>

#include <llvm/IR/Constants.h>

#include "System.h"
#include "Utility.h"

ZulValue nullzul{nullptr, -1};

std::map<int, std::string> type_name_map = {
        {0, "논리"},
        {1, "글자"},
        {2, "수"},
        {3, "실수"},
        {4, "글"},
};

std::string get_type_name(int type_id) {
    if (type_id < 0)
        return "없음";
    int ptr_cnt = type_id / TYPE_COUNTS;
    int ac_type = type_id % TYPE_COUNTS;
    std::string ret;
    ret.reserve(type_name_map[ac_type].size() + ptr_cnt * 2);
    ret.append(type_name_map[ac_type]);
    for (int i = 0; i < ptr_cnt; ++i) {
        ret.append("[]");
    }
    return ret;
}

llvm::Constant *get_const_zero(llvm::LLVMContext &context, int type_id) {
    return get_const_zero(get_llvm_type(context, type_id), type_id);
}

llvm::Constant *get_const_zero(llvm::Type *llvm_type, int type_id) {
    switch (type_id) {
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

llvm::Type *get_llvm_type(llvm::LLVMContext &context, int type_id) {
    if (type_id > TYPE_COUNTS) {
        return llvm::PointerType::getUnqual(context);
    }
    switch (type_id) {
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

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id) {
    bool cast = true;
    if (target.second < 0)
        return false;
    if (dest_type_id == FLOAT_TYPEID) {
        target.first = zulctx.builder.CreateSIToFP(target.first, llvm::Type::getDoubleTy(*zulctx.context));
    } else if (dest_type_id == BOOL_TYPEID) {
        if (target.second == FLOAT_TYPEID) {
            target.first = zulctx.builder.CreateFCmpONE(target.first, get_const_zero(*zulctx.context, 0));
        } else {
            target.first = zulctx.builder.CreateICmpNE(target.first, get_const_zero(*zulctx.context, 0));
        }
    } else if (BOOL_TYPEID < dest_type_id && dest_type_id < FLOAT_TYPEID) {
        auto dest_type = get_llvm_type(*zulctx.context, dest_type_id);
        if (target.second == FLOAT_TYPEID) {
            target.first = zulctx.builder.CreateFPToSI(target.first, dest_type);
        } else {
            target.first = zulctx.builder.CreateIntCast(target.first, dest_type, target.second != BOOL_TYPEID);
        }
    } else {
        cast = false;
    }
    return cast;
}

llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op) {
    switch (op.value) {
        case tok_add:
            return zulctx.builder.CreateAdd(lhs, rhs);
        case tok_sub:
            return zulctx.builder.CreateSub(lhs, rhs);
        case tok_mul:
            return zulctx.builder.CreateMul(lhs, rhs);
        case tok_div:
            return zulctx.builder.CreateSDiv(lhs, rhs);
        case tok_mod:
            return zulctx.builder.CreateSRem(lhs, rhs);
        case tok_bitand:
            return zulctx.builder.CreateAnd(lhs, rhs);
        case tok_bitor:
            return zulctx.builder.CreateOr(lhs, rhs);
        case tok_bitxor:
            return zulctx.builder.CreateXor(lhs, rhs);
        case tok_lshift:
            return zulctx.builder.CreateShl(lhs, rhs);
        case tok_rshift:
            return zulctx.builder.CreateAShr(lhs, rhs); //C++ Signed Shift Right 를 따름
        case tok_eq:
            return zulctx.builder.CreateICmpEQ(lhs, rhs);
        case tok_ineq:
            return zulctx.builder.CreateICmpNE(lhs, rhs);
        case tok_gt:
            return zulctx.builder.CreateICmpSGT(lhs, rhs);
        case tok_gteq:
            return zulctx.builder.CreateICmpSGE(lhs, rhs);
        case tok_lt:
            return zulctx.builder.CreateICmpSLT(lhs, rhs);
        case tok_lteq:
            return zulctx.builder.CreateICmpSLE(lhs, rhs);
        default:
            System::logger.log_error(op.loc, op.word_size, "해당 연산자를 수 타입에 적용할 수 없습니다");
            return nullptr;
    }
}

llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op) {
    switch (op.value) {
        case tok_add:
            return zulctx.builder.CreateFAdd(lhs, rhs);
        case tok_sub:
            return zulctx.builder.CreateFSub(lhs, rhs);
        case tok_mul:
            return zulctx.builder.CreateFMul(lhs, rhs);
        case tok_div:
            return zulctx.builder.CreateFDiv(lhs, rhs);
        case tok_mod:
            return zulctx.builder.CreateFRem(lhs, rhs);
        case tok_eq:
            return zulctx.builder.CreateFCmpOEQ(lhs, rhs);
        case tok_ineq:
            return zulctx.builder.CreateFCmpONE(lhs, rhs);
        case tok_gt:
            return zulctx.builder.CreateFCmpOGT(lhs, rhs);
        case tok_gteq:
            return zulctx.builder.CreateFCmpOGE(lhs, rhs);
        case tok_lt:
            return zulctx.builder.CreateFCmpOLT(lhs, rhs);
        case tok_lteq:
            return zulctx.builder.CreateFCmpOLE(lhs, rhs);
        default:
            System::logger.log_error(op.loc, op.word_size, "해당 연산자를 소수 타입에 적용할 수 없습니다");
            return nullptr;
    }
}

bool is_cmp(Token op) {
    switch (op) {
        case tok_eq:
        case tok_ineq:
        case tok_gt:
        case tok_gteq:
        case tok_lt:
        case tok_lteq:
            return true;
        default:
            return false;
    }
}

bool to_boolean_expr(ZulContext &zulctx, ZulValue &expr) {
    if (expr.second == FLOAT_TYPEID) {
        expr.first = zulctx.builder.CreateFCmpONE(expr.first, llvm::ConstantFP::get(expr.first->getType(), 0));
    } else if (expr.second != BOOL_TYPEID) {
        auto zero = get_const_zero(*zulctx.context, expr.second);
        if (zero == nullptr) //bool형으로 캐스팅 불가능한 경우
            return false;
        expr.first = zulctx.builder.CreateICmpNE(expr.first, zero);
    }
    return true;
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
    return (0x1100 <= c && c <= 0x11FF) || (0x3130 <= c && c <= 0x318F) || (0xA960 <= c && c <= 0xA97F) ||
           (0xAC00 <= c && c <= 0xD7AF) || (0xD7B0 <= c && c <= 0xD7FF);
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