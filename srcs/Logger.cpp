#include <iostream>
#include "Utility.h"
#include "Logger.h"

using std::pair;
using std::string;
using std::unordered_map;
using std::string_view;
using std::clog;

Logger::Logger() : error_flag(false) {
    line_map.reserve(70);
}

Logger::~Logger() {
    flush();
}

void Logger::log_error(pair<int, int> loc, unsigned word_size, string_view msg) {
    buffer.emplace(loc, word_size, string(msg));
    error_flag = true;
}

void Logger::log_error(pair<int, int> loc, unsigned word_size, const std::initializer_list<std::string_view> &msgs) {
    //매모리의 반복 재할당을 막기 위해 initializer_list로 받고 한 번에 할당
    string str;
    unsigned size = 0;
    for (const auto &x: msgs) {
        size += x.size();
    }
    str.reserve(size);
    for (const auto &x: msgs) {
        str.append(x);
    }
    buffer.emplace(loc, word_size, std::move(str));
    error_flag = true;
}

void Logger::register_line(int line_num, string &&line) {
    line_map.emplace(line_num, std::move(line));
}

int Logger::get_byte_count(int c) {
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 1;
}

string Logger::highlight(std::string_view str, int col, unsigned word_size) {
    string ret;
    ret.reserve(str.size());
    int idx = 0;
    for (int i = 0; i < col; ++i) {
        int cnt = get_byte_count(str[idx]);
        if (cnt == 1) {
            if (str[idx] == '\t')
                ret.push_back('\t');
            else
                ret.push_back(' ');
        } else {
            ret.push_back('\xE3');
            ret.push_back('\x80');
            ret.push_back('\x80');
        }
        idx += cnt;
    }
    ret.push_back('^');
    if (word_size == 0)
        return ret;

    int c = get_byte_count(str[idx]);
    if (c > 1)
        ret.push_back('~');
    word_size -= c;
    idx += c;
    int m_byte = 0;
    for (int i = 0; i < word_size;) {
        int cnt = get_byte_count(str[idx]);
        if (cnt == 1) {
            ret.push_back('~');
        } else {
            m_byte++;
        }
        idx += cnt;
        i += cnt;
    }
    m_byte += (m_byte + 1) / 2 + m_byte / 8;
    for (int i = 0; i < m_byte; ++i) {
        ret.push_back('~');
    }
    return ret;
}

void Logger::flush() {
    while (!buffer.empty()) {
        auto &log = buffer.top();
        clog << source_name << ' ' << log.row << ':' << log.col << ": 에러: " << log.msg << '\n';
        clog.width(5);
        clog << log.row << " | " << line_map[log.row]
             << "\n      | " << highlight(line_map[log.row], log.col - 1, log.word_size) << '\n';
        buffer.pop();
    }
    line_map.clear();
}

void Logger::set_error() {
    error_flag = true;
}

bool Logger::has_error() const {
    return error_flag;
}

void Logger::set_source_name(const string &name) {
    source_name = name;
}