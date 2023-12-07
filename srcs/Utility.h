#ifndef ZULLANG_UTILITY_H
#define ZULLANG_UTILITY_H

#include <utility>
#include <map>
#include <string>
#include <functional>

#include <llvm/IR/Instructions.h>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

#include "ZulContext.h"
#include "Lexer.h"

#define BOOL_TYPEID 0 //기본 타입의 시작 번호
#define FLOAT_TYPEID 3 //기본 타입의 끝 번호
#define TYPE_COUNTS 5 //기본 타입의 개수
#define ENTRY_FN_NAME "main"

extern ZulValue nullzul;

template<typename T>
struct Capture {
    T value;
    std::pair<int, int> loc;
    unsigned word_size;

    Capture(T value, std::pair<int, int> loc, unsigned word_size) :
            value(std::move(value)), loc(loc), word_size(word_size) {}

    Capture(Capture<T> &&other) noexcept {
        value = std::move(other.value);
        loc = std::move(other.loc);
        word_size = other.word_size;
    }
};

template<typename T>
Capture<T> make_capture(T value, Lexer &lexer) {
    return Capture(std::move(value), lexer.get_token_start_loc(), lexer.get_word().size());
}

struct Guard {
    const std::function<void()> target;
    explicit Guard (std::function<void()> target) : target(std::move(target)) {}

    ~Guard() {
        target();
    }
};

std::string get_type_name(int type_id);

llvm::Constant *get_const_zero(llvm::Type *llvm_type, int type_id);

llvm::Constant *get_const_zero(llvm::LLVMContext &context, int type_id);

llvm::Type *get_llvm_type(llvm::LLVMContext &context, int type_id);

bool create_cast(ZulContext &zulctx, ZulValue &target, int dest_type_id);

llvm::Value *create_int_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

llvm::Value *create_float_operation(ZulContext &zulctx, llvm::Value *lhs, llvm::Value *rhs, Capture<Token> &op);

bool is_cmp(Token op);

bool to_boolean_expr(ZulContext &zulctx, ZulValue &expr);

int get_byte_count(int c);

bool iskor(int c);

bool isnum(int c);

bool iskornum(int c);

std::string token_to_string(Token token); //토큰을 문자열로 출력

#endif //ZULLANG_UTILITY_H
