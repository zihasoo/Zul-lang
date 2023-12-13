
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef ZULLANG_SYSTEM_H
#define ZULLANG_SYSTEM_H

#define ZULLANG_VERSION "1.0.3"

#include <string>
#include <iostream>
#include "llvm/Support/CommandLine.h"
#include "Logger.h"

class System {
public:
    static Logger logger;

    static std::string source_base_name;

    static llvm::cl::opt<std::string> source_name;

    static llvm::cl::opt<std::string> output_name;

    static llvm::cl::opt<bool> opt_compile;

    static llvm::cl::opt<bool> opt_assembly;

    static void parse_arg(int argc, char **argv);

private:
    static llvm::cl::OptionCategory zul_opt_category;
};


#endif //ZULLANG_SYSTEM_H
