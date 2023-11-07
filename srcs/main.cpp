#include <iostream>
#include <string>

#include "System.h"
#include "Lexer.h"

using namespace std;

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    Lexer lexer(System::source_name);

    Lexer::Token tok;
    while (true) {
        tok = lexer.get_token();
        if (tok == Lexer::tok_eof)
            break;
        System::logger.log_error(lexer.get_token_start_loc(), lexer.get_word().size(), Lexer::token_to_string(tok));
    }
    return 0;
}
