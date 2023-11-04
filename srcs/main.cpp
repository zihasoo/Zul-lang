#include <iostream>
#include <string>

#include "System.h"
#include "Lexer.h"


using namespace std;

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    cout << "source name: " << System::source_name << '\n';
    cout << "output name: " << System::output_name << '\n';
    Lexer lexer(System::source_name);
    Lexer::Token tok;
    while (tok != Lexer::tok_undefined) {
        tok = lexer.get_token();
        cout << '"' << Lexer::token_to_string(tok) << "\"\n";
    }
    return 0;
}
