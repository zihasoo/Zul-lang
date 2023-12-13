
//SPDX-FileCopyrightText: © 2023 Lee ByungYun <dlquddbs1234@gmail.com>
//SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/InitLLVM.h"

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

void write_module(Module *module) {
    if (auto original_main = module->getFunction("main")) {
        original_main->setName("old_main");
    }
    module->getFunction(ENTRY_FN_NAME)->setName("main");

    if (System::output_name.empty()) {
        auto dot_pos = System::source_name.rfind('.');
        System::output_name = System::source_name.substr(0, dot_pos) + (System::opt_assembly ? ".ll" : ".bc");
    }

    error_code EC;
    raw_fd_ostream output_file{System::output_name, EC};

    if (System::opt_assembly) {
        module->print(output_file, nullptr);
    } else {
        WriteBitcodeToFile(*module, output_file);
    }
}

void run_jit(unique_ptr<LLVMContext> context, unique_ptr<Module> module) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    ExitOnError ExitOnErr;

    ExitOnErr.setBanner(System::source_name + ": ");

    auto LLJIT = ExitOnErr(LLJITBuilder().create());
    auto TSM = ThreadSafeModule(std::move(module), std::move(context));

    ExitOnErr(LLJIT->addIRModule(std::move(TSM)));

    long long (*zul_main)() = ExitOnErr(LLJIT->lookup(ENTRY_FN_NAME)).toPtr<long long()>();
    zul_main();
}

void link_stdio(LLVMContext &context, Module &module) {
    auto buf_or_err = MemoryBuffer::getMemBuffer(StringRef((char *) zulstdio_bc, zulstdio_bc_len));
    auto stdio_module = getLazyBitcodeModule(buf_or_err->getMemBufferRef(), context);
    if (!stdio_module) {
        cerr << "에러: 줄랭 stdio 모듈 로드에 실패하였습니다. 컴파일러를 재설치하세요(복구 불가)\n";
        exit(1);
    }

    Linker linker(module);

    if (linker.linkInModule(std::move(stdio_module.get()))) {
        cerr << "에러: 줄랭 stdio 모듈 링킹에 실패하였습니다. 컴파일러를 재설치하세요(복구 불가)\n";
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    System::parse_arg(argc, argv);
#ifdef ZUL_DEBUG
    InitLLVM X(argc, argv);
#endif

    Parser parser{System::source_name};
    auto [context, module] = parser.parse();

    if (System::logger.has_error())
        return 1;

    if (System::opt_compile || System::opt_assembly) {
        link_stdio(*context, *module);
        write_module(module.get());
    } else {
        run_jit(std::move(context), std::move(module));
    }
    return 0;
}
