#include <iostream>
#include <string>

#include "System.h"
#include "Lexer.h"

using namespace std;

int main(int argc, char *argv[]) {

    Lexer lexer(System::source_name);

    Lexer::Token tok;
    while (tok != Lexer::tok_undefined) {
        tok = lexer.get_token();
        cout << '"' << Lexer::token_to_string(tok) << "\"\n";
    }

    return 0;
}
