#include "interpreter.h"
#include <iostream>
#include <string>
#include <memory>
#include "parser.h"

#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>

int run_interpreter(const std::string &source) {
    // parse source into AST
    Parser P(source);
    auto R = P.parse();
    if (!R.ok || !R.stmt) {
        std::cerr << "Parse error: " << R.error << "\n";
        return 1;
    }
    auto *ps = dynamic_cast<PrintStmt*>(R.stmt.get());
    if (!ps) {
        std::cerr << "Unsupported statement\n";
        return 1;
    }
    int intval = ps->Value;

    // Initialize native target for JIT
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Create LLJIT
    auto JOrErr = llvm::orc::LLJITBuilder().create();
    if (!JOrErr) {
        llvm::logAllUnhandledErrors(JOrErr.takeError(), llvm::errs(), "LLJIT create failed: ");
        return 2;
    }
    auto J = std::move(*JOrErr);

    // Prepare a thread-safe LLVM context + module
    llvm::orc::ThreadSafeContext TSCtx(std::make_unique<llvm::LLVMContext>());
    auto &ctx = *TSCtx.getContext();
    auto M = std::make_unique<llvm::Module>("oong_interpreter", ctx);
    llvm::IRBuilder<> builder(ctx);

    // declare printf
    std::vector<llvm::Type*> printf_arg_types;
    printf_arg_types.push_back(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)));
    auto printf_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), printf_arg_types, true);
    auto printf_func = llvm::Function::Create(printf_type, llvm::Function::ExternalLinkage, "printf", M.get());

    // create oong_main
    auto main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
    auto main_func = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "oong_main", M.get());
    auto bb = llvm::BasicBlock::Create(ctx, "entry", main_func);
    builder.SetInsertPoint(bb);

    auto fmt = builder.CreateGlobalStringPtr("%d\n");
    auto const_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), intval);
    builder.CreateCall(printf_func, {fmt, const_val});
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));

    if (llvm::verifyModule(*M, &llvm::errs())) {
        std::cerr << "Generated module is broken\n";
        return 3;
    }

    // Make module thread-safe and add to JIT
    llvm::orc::ThreadSafeModule TSM(std::move(M), std::move(TSCtx));
    // Add current process symbols so printf resolves to the C runtime
    if (auto GenOrErr = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(J->getDataLayout().getGlobalPrefix())) {
        auto Gen = std::move(*GenOrErr);
        J->getMainJITDylib().addGenerator(std::move(Gen));
    } else {
        llvm::logAllUnhandledErrors(GenOrErr.takeError(), llvm::errs(), "Failed to create DynamicLibrarySearchGenerator: ");
    }

    if (auto Err = J->addIRModule(std::move(TSM))) {
        llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "Failed to add IR module: ");
        return 4;
    }

    // Lookup symbol and run. LLJIT::lookup returns an ExecutorAddr directly.
    auto Addr = llvm::cantFail(J->lookup("oong_main"));
    // Convert ExecutorAddr to a callable pointer for an in-process JIT.
    using MainFnType = int();
    auto *mainPtr = Addr.toPtr<MainFnType>();
    int rc = mainPtr();
    (void)rc; // result printed by printf already
    return 0;
}
