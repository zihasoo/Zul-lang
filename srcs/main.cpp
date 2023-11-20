#include <iostream>
#include <string>

#include "System.h"
#include "Parser.h"
#include "Utility.h"

using namespace std;

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    Parser parser{System::source_name};
    parser.parse();

    return 0;
}
