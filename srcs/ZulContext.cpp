
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "ZulContext.h"

using llvm::Type;
using llvm::PointerType;
using llvm::ConstantInt;
using llvm::ConstantFP;
using llvm::ConstantPointerNull;
using llvm::Value;
using llvm::Constant;

const ZulValue nullzul{nullptr, -1};

std::string id_to_name(int type_id) {
    static std::map<int, std::string> type_name_map = {
        {ID_BOOL,  "논리"},
        {ID_CHAR,  "글자"},
        {ID_INT,   "수"},
        {ID_FLOAT, "실수"},
    };

    if (type_id < 0)
        return "없음";
    int ptr_cnt = type_id / ID_MAX;
    int type = type_id % ID_MAX;
    std::string ret;
    const std::string& name = type_name_map[type];
    ret.reserve(name.size() + ptr_cnt * 2);
    ret.append(name);
    if (ptr_cnt == 1) {
        ret.append("[]");
    } else if (ptr_cnt > 1){
        ret.append("*");
    }
    return ret;
}

bool ZulContext::is_declared(const std::string &var_name) const {
    return global_var_map.contains(var_name) || local_var_map.contains(var_name);
}

void ZulContext::remove_scope_vars() {
    //스코프 벗어날 때 생성한 변수들 맵에서 삭제 (IR코드에는 남아있음. 맵에서 지워서 접근만 막는 것)
    auto &cur_scope_vars = scope_stack.top();
    for (auto &name: cur_scope_vars) {
        local_var_map.erase(name);
    }
    scope_stack.pop();
}

Constant* ZulContext::zero_of(int type_id) const {
    Type* type = id_to_type(type_id);
    switch (type_id) {
        case ID_BOOL:
        case ID_CHAR:
        case ID_INT:
            return ConstantInt::get(type, 0, true);
        case ID_FLOAT:
            return ConstantFP::get(type, 0);
        case 4:
            return ConstantPointerNull::get(cast<PointerType>(type));
        default:
            return nullptr;
    }
}

Type* ZulContext::id_to_type(int type_id) const {
    if (type_id > ID_MAX) {
        return PointerType::getUnqual(*context);
    }
    switch (type_id) {
        case ID_BOOL:
            return Type::getInt1Ty(*context);
        case ID_CHAR:
            return Type::getInt8Ty(*context);
        case ID_INT:
            return Type::getInt64Ty(*context);
        case ID_FLOAT:
            return Type::getDoubleTy(*context);
        default:
            return Type::getVoidTy(*context);
    }
}

bool ZulContext::create_cast(ZulValue &source, int dest_type_id) {
    if (source.id < 0)
        return false;
    if (dest_type_id == ID_FLOAT) {
        source.value = builder.CreateSIToFP(source.value, Type::getDoubleTy(*context));
    } else if (dest_type_id == ID_BOOL) {
        if (source.id == ID_FLOAT) {
            source.value = builder.CreateFCmpONE(source.value, zero_of(ID_BOOL));
        } else {
            source.value = builder.CreateICmpNE(source.value, zero_of(ID_BOOL));
        }
    } else if (ID_BOOL < dest_type_id && dest_type_id < ID_FLOAT) {
        Type* dest_type = id_to_type(dest_type_id);
        if (source.id == ID_FLOAT) {
            source.value = builder.CreateFPToSI(source.value, dest_type);
        } else {
            source.value = builder.CreateIntCast(source.value, dest_type, source.id != ID_BOOL);
        }
    } else {
        return false;
    }
    return true;
}

Value * ZulContext::create_int_operation(Value *lhs, Value *rhs, Capture<Token> &op) {
    switch (op.value) {
        case tok_add:
            return builder.CreateAdd(lhs, rhs);
        case tok_sub:
            return builder.CreateSub(lhs, rhs);
        case tok_mul:
            return builder.CreateMul(lhs, rhs);
        case tok_div:
            return builder.CreateSDiv(lhs, rhs);
        case tok_mod:
            return builder.CreateSRem(lhs, rhs);
        case tok_bitand:
            return builder.CreateAnd(lhs, rhs);
        case tok_bitor:
            return builder.CreateOr(lhs, rhs);
        case tok_bitxor:
            return builder.CreateXor(lhs, rhs);
        case tok_lshift:
            return builder.CreateShl(lhs, rhs);
        case tok_rshift:
            return builder.CreateAShr(lhs, rhs); //C++ Signed Shift Right 를 따름
        case tok_eq:
            return builder.CreateICmpEQ(lhs, rhs);
        case tok_ineq:
            return builder.CreateICmpNE(lhs, rhs);
        case tok_gt:
            return builder.CreateICmpSGT(lhs, rhs);
        case tok_gteq:
            return builder.CreateICmpSGE(lhs, rhs);
        case tok_lt:
            return builder.CreateICmpSLT(lhs, rhs);
        case tok_lteq:
            return builder.CreateICmpSLE(lhs, rhs);
        default:
            System::logger.log_error(op.loc, op.word_size, {"해당 연산자를 \"", id_to_name(ID_INT) ,"\" 타입에 적용할 수 없습니다"});
            return nullptr;
    }
}

Value * ZulContext::create_float_operation(Value *lhs, Value *rhs, Capture<Token> &op) {
    switch (op.value) {
        case tok_add:
            return builder.CreateFAdd(lhs, rhs);
        case tok_sub:
            return builder.CreateFSub(lhs, rhs);
        case tok_mul:
            return builder.CreateFMul(lhs, rhs);
        case tok_div:
            return builder.CreateFDiv(lhs, rhs);
        case tok_mod:
            return builder.CreateFRem(lhs, rhs);
        case tok_eq:
            return builder.CreateFCmpOEQ(lhs, rhs);
        case tok_ineq:
            return builder.CreateFCmpONE(lhs, rhs);
        case tok_gt:
            return builder.CreateFCmpOGT(lhs, rhs);
        case tok_gteq:
            return builder.CreateFCmpOGE(lhs, rhs);
        case tok_lt:
            return builder.CreateFCmpOLT(lhs, rhs);
        case tok_lteq:
            return builder.CreateFCmpOLE(lhs, rhs);
        default:
            System::logger.log_error(op.loc, op.word_size, {"해당 연산자를 \"", id_to_name(ID_FLOAT) ,"\" 타입에 적용할 수 없습니다"});
            return nullptr;
    }
}

bool ZulContext::to_boolean_expr(ZulValue &expr) {
    if (expr.id == ID_FLOAT) {
        expr.value = builder.CreateFCmpONE(expr.value, ConstantFP::get(expr.value->getType(), 0));
    } else if (expr.id != ID_BOOL) {
        Constant* zero = zero_of(expr.id);
        if (zero == nullptr) //bool형으로 캐스팅 불가능한 경우
            return false;
        expr.value = builder.CreateICmpNE(expr.value, zero);
    }
    return true;
}
