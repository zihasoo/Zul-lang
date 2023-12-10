#ifndef ZULLANG_LOGGER_H
#define ZULLANG_LOGGER_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <queue>
#include <string_view>

class Logger {
public:
    struct LogInfo {
        int row, col;
        unsigned word_size;
        std::string msg;

        LogInfo() = default;

        LogInfo(std::pair<int, int> loc, unsigned word_size, std::string &&msg)
                : row(loc.first), col(loc.second), word_size(word_size), msg(std::move(msg)) {}

        bool operator>(const LogInfo &other) const {
            if (row == other.row) return col > other.col;
            return row > other.row;
        }
    };

    Logger();

    ~Logger();

    void set_source_name(const std::string &name);

    void log_error(std::pair<int, int> loc, unsigned word_size, std::string_view msg);

    void log_error(std::pair<int, int> loc, unsigned word_size, const std::initializer_list<std::string_view> &msgs);

    void register_line(int line_num, std::string &&line);

    void flush();

    void set_error();

    [[nodiscard]] bool has_error() const;

private:
    std::string source_name;

    std::priority_queue<LogInfo, std::vector<LogInfo>, std::greater<>> buffer;

    std::unordered_map<int, std::string> line_map;

    bool error_flag;

    static int get_byte_count(int c);

    static std::string highlight(std::string_view str, int col, unsigned word_size);
};


#endif //ZULLANG_LOGGER_H
