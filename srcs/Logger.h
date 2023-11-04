#ifndef ZULLANG_LOGGER_H
#define ZULLANG_LOGGER_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sstream>

class Logger {
public:
    Logger();

    void set_source_name(const std::string& name);

    void log_error(std::pair<int, int> loc, int word_size, const std::string &msg);

    void register_line(int line_num, const std::string &line);

    void flush();

    bool has_error();

private:
    std::string source_name;

    std::stringstream buffer;

    std::unordered_map<int, std::string> line_map;

    bool error_flag;

    static std::string indent_by_count(int count);

    static std::string tilde(int count);

    static constexpr int buffer_size = 50;
};


#endif //ZULLANG_LOGGER_H
