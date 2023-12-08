#include <iostream>
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

#include "System.h"
#include "Parser.h"

using namespace std;

//extern "C" __declspec(dllexport) void ps(char * x) {
//    puts(x);
//}

void run_jit(std::unique_ptr<llvm::LLVMContext> context, std::unique_ptr<llvm::Module> module) {
    if (System::logger.has_error())
        return;

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::ExitOnError ExitOnErr;

    ExitOnErr.setBanner(System::source_name + ": ");

    auto J = ExitOnErr(llvm::orc::LLJITBuilder().create());
    auto M = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));

    ExitOnErr(J->addIRModule(std::move(M)));

    auto zul_main_addr = ExitOnErr(J->lookup(ENTRY_FN_NAME));
    long long (*zul_main)() = zul_main_addr.toPtr<long long()>();
    zul_main();
}

void print_ll(llvm::Module* module) {
    std::error_code EC;
    llvm::raw_fd_ostream output_file{System::output_name, EC};
    module->print(output_file, nullptr);
}

int main(int argc, char *argv[]) {
    llvm::InitLLVM X(argc, argv);
    System::parse_arg(argc, argv);

    Parser parser{System::source_name};
    auto [context, module] = parser.parse();

    print_ll(module.get());
    run_jit(std::move(context), std::move(module));

    return 0;
}
