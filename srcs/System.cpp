#include "System.h"

using std::string;
using std::cerr;

string System::source_name = string();
string System::output_name = string();
Logger System::logger = Logger();

void System::parse_arg(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'o' && i + 1 < argc && argv[i + 1][0] != '-') {
                if (!output_name.empty()) {
                    cerr << "에러: output 파일이 여러개입니다.\n";
                    exit(1);
                }
                output_name = argv[i + 1];
                i++;
            } else {
                cerr << "에러: 프로그램 인수가 잘못되었습니다.\n";
                exit(1);
            }
        } else if (source_name.empty()){
            source_name = argv[i];
        }
        else {
            cerr << "에러: 줄랭 컴파일러는 아직 여러 개의 파일을 인자로 받을 수 없습니다.\n";
            exit(1);
        }
    }
    if (source_name.empty()) {
        cerr << "에러: 소스 파일이 주어지지 않았습니다.\n";
        exit(1);
    }

    if (output_name.empty()) {
        output_name = source_name.substr(0, source_name.rfind('.') + 1) + "ll";
    }

    logger.set_source_name(source_name);
}