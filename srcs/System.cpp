
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "llvm/TargetParser/Host.h"
#include "System.h"

using namespace llvm;

std::string System::source_base_name = std::string();

std::string System::target_triple = sys::getProcessTriple();

cl::OptionCategory System::zul_opt_category = cl::OptionCategory("zul options");

cl::opt<std::string> System::source_name = cl::opt<std::string>(cl::Positional, cl::desc("<줄랭 소스파일>"), cl::cat(zul_opt_category));

cl::opt<std::string> System::output_name = cl::opt<std::string>("o", cl::desc("아웃풋 파일 이름"), cl::value_desc("파일 이름"), cl::cat(zul_opt_category));

cl::opt<bool> System::opt_compile = cl::opt<bool>("c", cl::desc("bitcode 파일로 컴파일"), cl::cat(zul_opt_category));

cl::opt<bool> System::opt_assembly = cl::opt<bool>("S", cl::desc("ll 파일로 컴파일"), cl::cat(zul_opt_category));

Logger System::logger = Logger();

void System::parse_arg(int argc, char **argv) {
    cl::HideUnrelatedOptions(zul_opt_category);

    cl::SetVersionPrinter([](raw_ostream &out) {
        out << "zul-lang compiler version " << ZULLANG_VERSION << '\n';
        out << "Target: " << target_triple << '\n';
        out << "made by Lee ByungYun 2023\n";
    });

    cl::ParseCommandLineOptions(argc, argv, std::string("줄랭 컴파일러 ") + ZULLANG_VERSION + "\n");

    if (source_name.empty()) {
        std::cerr << "에러: 소스 파일이 주어지지 않았습니다.\n";
        exit(1);
    }

    if (!source_name.ends_with(".zul") && !source_name.ends_with(".줄")) {
        std::cerr << "에러: 알 수 없는 확장자입니다. \"zul\" 또는 \"줄\" 확장자가 필요합니다.\n";
        exit(1);
    }

    auto s_pos = source_name.rfind('\\');
    if (s_pos == std::string::npos)
        s_pos = source_name.rfind('/');

    if (s_pos == std::string::npos) {
        source_base_name = source_name;
    } else {
        source_base_name = source_name.substr(s_pos + 1);
    }

    logger.set_source_name(source_base_name);
}
