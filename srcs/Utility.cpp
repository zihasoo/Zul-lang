
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Utility.h"

bool iscmp(Token op) {
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
