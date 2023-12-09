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
using llvm::cl::opt;

OptionCategory System::zul_opt_category = OptionCategory("zul options");
string System::source_base_name = string();
opt<string> System::source_name = opt<string>(Positional, desc("<줄랭 소스파일>"), cat(zul_opt_category));
opt<string> System::output_name = opt<string>("o", desc("아웃풋 파일 이름"), value_desc("파일 이름"), cat(zul_opt_category));
opt<bool> System::compile = opt<bool>("c", desc("ll 파일로 컴파일"), cat(zul_opt_category));

Logger System::logger = Logger();

void System::parse_arg(int argc, char **argv) {
    HideUnrelatedOptions(zul_opt_category);
    ParseCommandLineOptions(argc, argv, "줄랭 컴파일러 v1.0.0\n");
    if (source_name.empty()) {
        cerr << "에러: 소스 파일이 주어지지 않았습니다.\n";
        exit(1);
    }

    if (!source_name.ends_with(".zul") && !source_name.ends_with(".줄")) {
        cerr << "에러: 알 수 없는 확장자입니다. \"zul\" 또는 \"줄\" 확장자가 필요합니다.\n";
        exit(1);
    }

    if (output_name.empty()) {
        output_name = source_name.substr(0, source_name.rfind('.') + 1) + "ll";
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
