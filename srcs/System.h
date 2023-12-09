#ifndef ZULLANG_SYSTEM_H
#define ZULLANG_SYSTEM_H

#include <string>
#include <iostream>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"

#include "Logger.h"

class System {
public:
    static Logger logger;

    static std::string source_base_name;

    static llvm::cl::opt<std::string> source_name;

    static llvm::cl::opt<std::string> output_name;

    static llvm::cl::opt<bool> compile;

    static void parse_arg(int argc, char **argv);

private:
    static llvm::cl::OptionCategory zul_opt_category;
};


#endif //ZULLANG_SYSTEM_H
