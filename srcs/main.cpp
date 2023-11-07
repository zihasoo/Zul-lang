#include <iostream>
#include <string>

#include "System.h"
#include "Lexer.h"

using namespace std;

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    Lexer lexer(System::source_name);

    Lexer::Token tok;
    while (tok != Lexer::tok_eof) {
        tok = lexer.get_token();
        cout << Lexer::token_to_string(tok) << "\n";
        //System::logger.log_error(lexer.get_token_loc(), max(1, lexer.get_cur_loc().second - lexer.get_token_loc().second), "");
    }
    //System::logger.flush();
    return 0;
}
