
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULLANG_UTILITY_H
#define ZULLANG_UTILITY_H

#include <utility>

#include "Lexer.h"

#define ENTRY_FN_NAME "시작" //진입점 함수 이름
#define STDIN_NAME "입"
#define STDOUT_NAME "출"

template<typename T>
struct Capture {
    T value;
    std::pair<int, int> loc;
    unsigned word_size;

    Capture(T value, std::pair<int, int> loc, unsigned word_size) :
            value(std::move(value)), loc(loc), word_size(word_size) {}
};

template<typename T>
Capture<T> make_capture(T value, Lexer &lexer) {
    return Capture(std::move(value), lexer.get_token_loc(), lexer.get_word().size());
}

template <typename F>
class [[nodiscard]] Guard {
    F f;

public:
    explicit Guard(F func) : f(std::move(func)) {}
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;
    ~Guard() { f(); }
};

bool iscmp(Token op);

bool iskor(int c);

bool isnum(int c);

bool iskornum(int c);

#endif //ZULLANG_UTILITY_H
