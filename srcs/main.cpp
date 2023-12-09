#include <iostream>
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"

#include "System.h"
#include "Parser.h"
#include "Zulstdio.h"

using std::string;
using std::unique_ptr;
using std::pair;
using std::vector;
using std::error_code;
using std::cerr;

using llvm::raw_fd_ostream;
using llvm::Module;
using llvm::LLVMContext;
using llvm::StringRef;
using llvm::MemoryBuffer;
using llvm::Linker;
using llvm::InitLLVM;
using llvm::InitializeNativeTarget;
using llvm::InitializeNativeTargetAsmPrinter;
using llvm::ExitOnError;
using llvm::orc::LLJITBuilder;
using llvm::orc::ThreadSafeModule;

void print_ll(Module *module) {
    error_code EC;
    raw_fd_ostream output_file{System::output_name, EC};
    module->print(output_file, nullptr);
}

void run_jit(unique_ptr<LLVMContext> context, unique_ptr<Module> module) {
    if (System::logger.has_error())
        return;

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    ExitOnError ExitOnErr;

    ExitOnErr.setBanner(System::source_name + ": ");

    auto J = ExitOnErr(LLJITBuilder().create());
    auto M = ThreadSafeModule(std::move(module), std::move(context));

    ExitOnErr(J->addIRModule(std::move(M)));

    auto zul_main_addr = ExitOnErr(J->lookup(ENTRY_FN_NAME));
    long long (*zul_main)() = zul_main_addr.toPtr<long long()>();
    zul_main();
}

void link_stdio(LLVMContext &context, Module &module) {
    auto buf_or_err = MemoryBuffer::getMemBuffer(StringRef((char *) zulstdio_bc, zulstdio_bc_len));
    auto stdio_module = getLazyBitcodeModule(buf_or_err->getMemBufferRef(), context);
    if (!stdio_module) {
        cerr << "줄랭 stdio 모듈 로드에 실패하였습니다. 컴파일러를 재설치하세요(복구 불가)\n";
        exit(1);
    }

    Linker linker(module);

    if (linker.linkInModule(std::move(stdio_module.get()))) {
        cerr << "줄랭 stdio 모듈 링킹에 실패하였습니다. 컴파일러를 재설치하세요(복구 불가)\n";
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
    InitLLVM X(argc, argv);

    Parser parser{System::source_name};
    auto [context, module] = parser.parse();

    if (System::logger.has_error())
        return 1;

    //link_stdio(*context, *module);

    if (System::compile) {
        print_ll(module.get());
    } else {
        run_jit(std::move(context), std::move(module));
    }

    return 0;
}
