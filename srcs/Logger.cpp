#include <iostream>
#include "Logger.h"

using std::pair;
using std::string;
using std::unordered_map;
using std::clog;

Logger::Logger() : error_flag(false) {
    line_map.reserve(buffer_size + 5);
}

void Logger::log_error(pair<int, int> loc, int word_size, const string &msg) {
    clog << source_name << ' ' << loc.first << ':' << loc.second << ": 에러: " << msg << '\n';
    clog.width(5);
    clog << loc.first << " | " << line_map[loc.first] << '\n'
           << "      | " << indent(loc.second - 1) << "^"
           << tilde(word_size - 1) << '\n';
    error_flag = true;
}

void Logger::register_line(int line_num, const string &line) {
    if (!line_map.contains(line_num))
        line_map.emplace(line_num, line);
    if (line_map.size() > buffer_size)
        flush();
}

string Logger::indent(int count) {
    string ret;
    ret.assign(count, ' ');
    return ret;
}

string Logger::tilde(int count) {
    string ret;
    ret.assign(count, '~');
    return ret;
}

void Logger::flush() {
    line_map.clear();
}

bool Logger::has_error() const {
    return error_flag;
}

void Logger::set_source_name(const string &name) {
    source_name = name;
}