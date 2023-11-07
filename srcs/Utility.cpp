#include "Utility.h"

bool iskor(int c) {
    return (0x3131 <= c && c <= 0x318E) || (0xAC00 <= c && c <= 0xD7FF);
}

bool isnum(int c) {
    return '0' <= c && c <= '9';
}

bool iskornum(int c) {
    return iskor(c) || isnum(c);
}
