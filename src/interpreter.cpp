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
    auto isNumber = [](const std::string& s) {
        if (s.empty()) return false;
        size_t i = 0;
        if (s[0] == '-') i = 1;
        bool dot = false;
        for (; i < s.size(); ++i) {
            if (s[i] == '.') {
                if (dot) return false;
                dot = true;
                continue;
            }
            if (!isdigit(s[i])) return false;
        }
        return true;
    };
    // parse source into AST
    Parser P(source);
    auto R = P.parse();
    if (!R.ok || !R.stmt) {
        std::cerr << "Interpreter Parse error: " << R.error << "\n";
        return 1;
    }


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

    // declare puts
    auto puts_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx))}, false);
    auto puts_func = llvm::Function::Create(puts_type, llvm::Function::ExternalLinkage, "puts", M.get());

    // create oong_main
    auto main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
    auto main_func = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "oong_main", M.get());
    auto bb = llvm::BasicBlock::Create(ctx, "entry", main_func);
    builder.SetInsertPoint(bb);

    // Execute all statements in Program
    std::string yellow = "\033[33m";
    std::string reset = "\033[0m";
    auto *prog = dynamic_cast<Program*>(R.stmt.get());
    if (prog) {
        // std::cerr << "[DEBUG] Program has " << prog->statements.size() << " statements\n";
        for (const auto& s : prog->statements) {
            if (auto ps = dynamic_cast<PrintStmt*>(s.get())) {
                // std::cerr << "[DEBUG] Executing PrintStmt with " << ps->args.size() << " args\n";
                std::string color = "";
                if (ps->origin == TokenKind::Tok_ConsoleError) {
                    color = "\033[31m"; // red
                } else if (ps->origin == TokenKind::Tok_ConsoleWarn) {
                    color = "\033[38;2;255;165;0m"; // orange-yellow
                } else if (ps->origin == TokenKind::Tok_ConsoleInfo) {
                    color = "\033[34m"; // blue
                } else if (ps->origin == TokenKind::Tok_ConsoleSuccess) {
                    color = "\033[32m"; // green
                }
                std::string result;
                for (size_t i = 0; i < ps->args.size(); ++i) {
                    std::string argStr;
                    if (auto lit = dynamic_cast<LiteralExpr*>(ps->args[i].get())) {
                        if (isNumber(lit->value)) {
                            argStr = yellow + lit->value + reset;
                        } else {
                            argStr = color + lit->value;
                        }
                    } else if (auto call = dynamic_cast<CallExpr*>(ps->args[i].get())) {
                        argStr = call->callee + "()";
                    } else {
                        argStr = "<unsupported expr>";
                    }
                    if (i > 0) result += " ";
                    result += argStr;
                }
                if (!color.empty()) {
                    result = color + result + reset;
                }
                auto out_str = builder.CreateGlobalStringPtr(result.c_str());
                builder.CreateCall(puts_func, {out_str});
            }
        }
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
    } else if (auto ps = dynamic_cast<PrintStmt*>(R.stmt.get())) {
        std::string color = "";
        if (ps->origin == TokenKind::Tok_ConsoleError) {
            color = "\033[31m";
        } else if (ps->origin == TokenKind::Tok_ConsoleWarn) {
            color = "\033[38;2;255;165;0m";
        } else if (ps->origin == TokenKind::Tok_ConsoleInfo) {
            color = "\033[34m";
        } else if (ps->origin == TokenKind::Tok_ConsoleSuccess) {
            color = "\033[32m";
        }
        std::string result;
        for (size_t i = 0; i < ps->args.size(); ++i) {
            std::string argStr;
            if (auto lit = dynamic_cast<LiteralExpr*>(ps->args[i].get())) {
                if (isNumber(lit->value) && ps->origin == TokenKind::Tok_Print) {
                    argStr = yellow + lit->value + reset;
                } else {
                    argStr = lit->value;
                }
            } else if (auto call = dynamic_cast<CallExpr*>(ps->args[i].get())) {
                argStr = call->callee + "()";
            } else {
                argStr = "<unsupported expr>";
            }
            if (i > 0) result += " ";
            result += argStr;
        }
        if (!color.empty()) {
            result = color + result + reset;
        }
        auto out_str = builder.CreateGlobalStringPtr(result.c_str());
        builder.CreateCall(puts_func, {out_str});
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
    } else {
        std::cerr << "Unsupported statement\n";
        return 1;
    }

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
