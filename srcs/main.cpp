#include <iostream>
#include <string>

#include "System.h"
#include "Parser.h"
#include "Utility.h"

#include <codecvt>
#include <locale>

using namespace std;

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    Parser parser{System::source_name};
    parser.parse();

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;

    cout << "[전역변수]\n";
    auto& vars = parser.get_global_var_map();
    for(auto& var : vars) {
        cout << convert.to_bytes(var.first) << ' ' << var.second << '\n';
    }
    cout << "\n[함수]\n";
    auto& funcs = parser.get_func_map();
    for(auto& func : funcs) {
        cout << convert.to_bytes(func.first) << '\n';
    }

    return 0;
}
