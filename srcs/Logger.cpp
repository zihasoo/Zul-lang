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

void Logger::log_error(const Logger::LogInfo &log_info) {
    buffer.push(log_info);
}

void Logger::register_line(int line_num, string &&line) {
    if (!line_map.contains(line_num))
        line_map.emplace(line_num, std::move(line));
    if (line_map.size() > max_line_map_size)
        flush();
}

string Logger::indent(string_view line, int col) {
    string ret;
    int u8cnt = 0;
    int byte;
    for (int i = 0; i - u8cnt * 2 < col; ++i) {
        byte = get_byte_count(line[i]);
        if (byte == 1) {
            ret.push_back(' ');
        }
        else {
            u8cnt++;
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
        clog << log.row << " | " << line_map[log.row] << '\n'
             << "      | " << indent(line_map[log.row], log.col - 1) << "^"
             << tilde(log.word_size - 1) << '\n';
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