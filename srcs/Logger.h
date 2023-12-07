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

    void log_error(std::pair<int, int> loc, unsigned word_size, std::string &&msg);

    void log_error(std::pair<int, int> loc, unsigned word_size, const std::initializer_list<std::string_view> &msgs);

    void log_error(LogInfo &&log_info);

    void register_line(int line_num, std::string &&line);

    void flush();

    bool has_error() const;

    static constexpr int max_line_map_size = 50;

private:
    std::string source_name;

    std::priority_queue<LogInfo, std::vector<LogInfo>, std::greater<>> buffer;

    std::unordered_map<int, std::string> line_map;

    bool error_flag;

    static std::string indent(std::string_view line, int col);

    static std::string tilde(unsigned count);
};


#endif //ZULLANG_LOGGER_H
