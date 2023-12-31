
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "llvm/TargetParser/Host.h"
#include "System.h"

using std::string;
using std::cerr;
using llvm::cl::Positional;
using llvm::cl::desc;
using llvm::cl::value_desc;
using llvm::cl::OptionCategory;
using llvm::cl::cat;
using llvm::cl::ParseCommandLineOptions;
using llvm::cl::HideUnrelatedOptions;
using llvm::cl::SetVersionPrinter;
using llvm::cl::opt;
using llvm::sys::getProcessTriple;

string System::source_base_name = string();

string System::target_triple = getProcessTriple();

OptionCategory System::zul_opt_category = OptionCategory("zul options");

opt<string> System::source_name = opt<string>(Positional, desc("<줄랭 소스파일>"), cat(zul_opt_category));

opt<string> System::output_name = opt<string>("o", desc("아웃풋 파일 이름"), value_desc("파일 이름"), cat(zul_opt_category));

opt<bool> System::opt_compile = opt<bool>("c", desc("bitcode 파일로 컴파일"), cat(zul_opt_category));

opt<bool> System::opt_assembly = opt<bool>("S", desc("ll 파일로 컴파일"), cat(zul_opt_category));

Logger System::logger = Logger();

void System::parse_arg(int argc, char **argv) {
    HideUnrelatedOptions(zul_opt_category);

    SetVersionPrinter([](llvm::raw_ostream &out) {
        out << "zul-lang compiler version " << ZULLANG_VERSION << '\n';
        out << "Target: " << target_triple << '\n';
        out << "made by Lee ByungYun 2023\n";
    });

    ParseCommandLineOptions(argc, argv, string("줄랭 컴파일러 ") + ZULLANG_VERSION + "\n");

    if (source_name.empty()) {
        cerr << "에러: 소스 파일이 주어지지 않았습니다.\n";
        exit(1);
    }

    if (!source_name.ends_with(".zul") && !source_name.ends_with(".줄")) {
        cerr << "에러: 알 수 없는 확장자입니다. \"zul\" 또는 \"줄\" 확장자가 필요합니다.\n";
        exit(1);
    }

    auto s_pos = source_name.rfind('\\');
    if (s_pos == string::npos)
        s_pos = source_name.rfind('/');

    if (s_pos == string::npos) {
        source_base_name = source_name;
    } else {
        source_base_name = source_name.substr(s_pos + 1);
    }

    logger.set_source_name(source_base_name);
}
