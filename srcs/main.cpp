#include <string>

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

#include "System.h"
#include "Parser.h"

using namespace std;

extern "C" __declspec(dllexport) void puti(long long x) {
    printf("%lld",x);
}

extern "C" __declspec(dllexport) void putd(double x) {
    printf("%lf", x);
}

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    Parser parser{System::source_name};
    parser.parse();

    if (System::logger.has_error())
        return 0;
#ifndef IS_MINGW
    llvm::InitLLVM X(argc, argv);

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::ExitOnError ExitOnErr;

    ExitOnErr.setBanner(System::source_name + ": ");

    auto J = ExitOnErr(llvm::orc::LLJITBuilder().create());
    auto M = llvm::orc::ThreadSafeModule(std::move(parser.get_zulctx().module), std::move(parser.get_zulctx().context));

    ExitOnErr(J->addIRModule(std::move(M)));

    auto zul_main_addr = ExitOnErr(J->lookup("main"));
    long long (*zul_main)() = zul_main_addr.toPtr<long long()>();

    zul_main();
#endif
    return 0;
}
