#ifndef ZULLANG_UTILITY_H
#define ZULLANG_UTILITY_H

#include <utility>
#include <map>
#include <string>
#include <string_view>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

#include "Lexer.h"

#define BOOL_TYPEID 0 //기본 타입의 시작 번호
#define FLOAT_TYPEID 3 //기본 타입의 끝 번호
#define ENTRY_FN_NAME "main"

extern std::map<int, std::string> type_name_map;

template<typename T>
struct Capture {
    T value;
    std::pair<int, int> loc;
    int word_size;

    Capture(T value, std::pair<int, int> loc, int word_size) :
            value(std::move(value)), loc(std::move(loc)), word_size(word_size) {}

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

llvm::Constant *get_const_zero(llvm::Type *llvm_type, int type_num);

llvm::Constant *get_const_zero(llvm::LLVMContext &context, int type_num);

llvm::Type *get_llvm_type(llvm::LLVMContext &context, int type_num);

int get_byte_count(int c);

bool iskor(int c);

bool isnum(int c);

bool iskornum(int c);

std::string token_to_string(Token token); //토큰을 문자열로 출력

#endif //ZULLANG_UTILITY_H
