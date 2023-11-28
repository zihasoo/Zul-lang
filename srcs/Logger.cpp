#include <iostream>
#include "Utility.h"
#include "Logger.h"

using std::pair;
using std::string;
using std::unordered_map;
using std::string_view;
using std::clog;

Logger::Logger() : error_flag(false) {
    line_map.reserve(max_line_map_size + 5);
}

Logger::~Logger() {
    flush();
}

void Logger::log_error(pair<int, int> loc, int word_size, string &&msg) {
    buffer.emplace(loc, word_size, std::move(msg));
    error_flag = true;
}

void Logger::log_error(pair<int, int> loc, int word_size, const std::initializer_list<std::string_view> &msgs) {
    //매모리의 반복 재할당을 막기 위해 initializer_list로 받고 한 번에 할당
    string str;
    int size = 0;
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

void Logger::log_error(Logger::LogInfo &&log_info) {
    buffer.push(std::move(log_info));
    error_flag = true;
}

void Logger::register_line(int line_num, string &&line) {
    line_map.emplace(line_num, std::move(line));
    if (line_map.size() > max_line_map_size)
        flush();
}

string Logger::indent(string_view line, int col) {
    string ret;
    int corr = 0;
    int byte;
    for (int i = 0; i - corr < col; ++i) {
        byte = get_byte_count(line[i]);
        if (byte == 1) {
            ret.push_back(' ');
        } else {
            corr += byte - 1;
            ret.push_back('\xE3');
            ret.push_back('\x80');
            ret.push_back('\x80');
            i += byte - 1;
        }
    }
    return ret;
}

string Logger::tilde(int count) {
    string ret;
    count = std::max(0, count);
    ret.assign(count, '~');
    return ret;
}

void Logger::flush() {
    while (!buffer.empty()) {
        auto &log = buffer.top();
        clog << source_name << ' ' << log.row << ':' << log.col << ": 에러: " << log.msg << '\n';
        clog.width(5);
        clog << log.row << " | " << line_map[log.row]
             << "      | " << indent(line_map[log.row], log.col - 1)
             << "^" << tilde(log.word_size - 1) << '\n';
        buffer.pop();
    }
    line_map.clear();
}

bool Logger::has_error() const {
    return error_flag;
}

void Logger::set_source_name(const string &name) {
    source_name = name;
}