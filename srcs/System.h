#ifndef ZULLANG_SYSTEM_H
#define ZULLANG_SYSTEM_H

#include <string>
#include <iostream>
#include "Logger.h"

class System {
public:
    static std::string source_name;
    static std::string output_name;
    static Logger logger;

    static void parse_arg(int argc, char **argv);
};


#endif //ZULLANG_SYSTEM_H
