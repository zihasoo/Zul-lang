
//SPDX-FileCopyrightText: © 2023 ByungYun Lee
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <sstream>

#include <llvm/IR/Constants.h>

#include "System.h"
#include "Utility.h"

using std::map;
using std::string;

using llvm::LLVMContext;
using llvm::ConstantInt;
using llvm::ConstantFP;
using llvm::ConstantPointerNull;
using llvm::Type;
using llvm::PointerType;
using llvm::Value;
using llvm::Constant;

ZulValue nullzul{nullptr, -1};

map<int, string> type_name_map = {
        {id_bool,  "논리"},
        {id_char,  "글자"},
        {id_int,   "수"},
        {id_float, "실수"},
};

string get_type_name(int type_id) {
    if (type_id < 0)
        return "없음";
    int ptr_cnt = type_id / TYPE_COUNTS;
    int type = type_id % TYPE_COUNTS;
    string ret;
    ret.reserve(type_name_map[type].size() + ptr_cnt * 2);
    ret.append(type_name_map[type]);
    if (ptr_cnt == 1) {
        ret.append("[]");
    } else if (ptr_cnt > 1){
        ret.append("*");
    }
    return ret;
}

Constant *get_const_zero(LLVMContext &context, int type_id) {
    return get_const_zero(get_llvm_type(context, type_id), type_id);
}

Constant *get_const_zero(Type *llvm_type, int type_id) {
    switch (type_id) {
        case id_bool:
        case id_char:
        case id_int:
            return ConstantInt::get(llvm_type, 0, true);
        case id_float:
            return ConstantFP::get(llvm_type, 0);
        case 4:
            return ConstantPointerNull::get(static_cast<PointerType *>(llvm_type));
        default:
            return nullptr;
    }
}

Type *get_llvm_type(LLVMContext &context, int type_id) {
    if (type_id > TYPE_COUNTS) {
        return PointerType::getUnqual(context);
    }
    switch (type_id) {
        case id_bool:
            return Type::getInt1Ty(context);
        case id_char:
            return Type::getInt8Ty(context);
        case id_int:
            return Type::getInt64Ty(context);
        case id_float:
            return Type::getDoubleTy(context);
        default:
            return Type::getVoidTy(context);
    }
}

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id) {
    bool cast = true;
    if (target.second < 0)
        return false;
    if (dest_type_id == id_float) {
        target.first = zulctx.builder.CreateSIToFP(target.first, Type::getDoubleTy(*zulctx.context));
    } else if (dest_type_id == id_bool) {
        if (target.second == id_float) {
            target.first = zulctx.builder.CreateFCmpONE(target.first, get_const_zero(*zulctx.context, 0));
        } else {
            target.first = zulctx.builder.CreateICmpNE(target.first, get_const_zero(*zulctx.context, 0));
        }
    } else if (id_bool < dest_type_id && dest_type_id < id_float) {
        auto dest_type = get_llvm_type(*zulctx.context, dest_type_id);
        if (target.second == id_float) {
            target.first = zulctx.builder.CreateFPToSI(target.first, dest_type);
        } else {
            target.first = zulctx.builder.CreateIntCast(target.first, dest_type, target.second != id_bool);
        }
    } else {
        cast = false;
    }
    return cast;
}

Value *create_int_operation(ZulContext &zulctx, Value *lhs, Value *rhs, Capture<Token> &op) {
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
            System::logger.log_error(op.loc, op.word_size, {"해당 연산자를 \"", type_name_map[id_int] ,"\" 타입에 적용할 수 없습니다"});
            return nullptr;
    }
}

Value *create_float_operation(ZulContext &zulctx, Value *lhs, Value *rhs, Capture<Token> &op) {
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
            System::logger.log_error(op.loc, op.word_size, {"해당 연산자를 \"", type_name_map[id_float] ,"\" 타입에 적용할 수 없습니다"});
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
    if (expr.second == id_float) {
        expr.first = zulctx.builder.CreateFCmpONE(expr.first, ConstantFP::get(expr.first->getType(), 0));
    } else if (expr.second != id_bool) {
        auto zero = get_const_zero(*zulctx.context, expr.second);
        if (zero == nullptr) //bool형으로 캐스팅 불가능한 경우
            return false;
        expr.first = zulctx.builder.CreateICmpNE(expr.first, zero);
    }
    return true;
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
