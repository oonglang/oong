#include "compiler.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <filesystem>
#include <optional>
#include <regex>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/IR/LegacyPassManager.h>

int run_compiler(const std::string &inputPath, const std::string &outPath) {
    std::string content;
    std::ifstream in(inputPath);
    if (!in) { std::cerr << "Could not open file: " << inputPath << "\n"; return 2; }
    content.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::regex r(R"(^\s*print\s*\(\s*([0-9]+)\s*\)\s*$)", std::regex::ECMAScript);
    std::smatch m;
    if (!content.empty() && std::regex_search(content, m, r) && m.size() >= 2) {
        std::string val = m[1].str();
        std::filesystem::path outp = outPath.empty() ? std::filesystem::path("a.exe") : std::filesystem::path(outPath);

        // Build LLVM module
        llvm::LLVMContext ctx;
        auto module = std::make_unique<llvm::Module>("oong_module", ctx);
        llvm::IRBuilder<> builder(ctx);

        std::vector<llvm::Type*> printf_arg_types;
        printf_arg_types.push_back(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)));
        auto printf_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), printf_arg_types, true);
        auto printf_func = llvm::Function::Create(printf_type, llvm::Function::ExternalLinkage, "printf", module.get());

        auto main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
        auto main_func = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module.get());
        auto bb = llvm::BasicBlock::Create(ctx, "entry", main_func);
        builder.SetInsertPoint(bb);

        auto format_str = builder.CreateGlobalStringPtr("%d\n");
        int intval = 0;
        try { intval = std::stoi(val); } catch (...) { intval = 0; }
        auto const_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), intval);
        builder.CreateCall(printf_func, {format_str, const_val});
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));

        // initialize native target
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        auto targetTriple = llvm::sys::getDefaultTargetTriple();
        module->setTargetTriple(targetTriple);

        std::string targetErr;
        const llvm::Target *target = llvm::TargetRegistry::lookupTarget(targetTriple, targetErr);
        if (!target) { std::cerr << "Target lookup failed: " << targetErr << "\n"; return 5; }

        std::string CPU = "generic";
        std::string features = "";
        llvm::TargetOptions opt;
        std::optional<llvm::Reloc::Model> RM = std::nullopt;
        std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(targetTriple, CPU, features, opt, RM));
        module->setDataLayout(targetMachine->createDataLayout());

        std::string objExt = (targetTriple.find("windows") != std::string::npos) ? ".obj" : ".o";
        std::filesystem::path objPath = outp.parent_path();
        if (objPath.empty()) objPath = std::filesystem::current_path();
        std::string stem = outp.stem().string();
        objPath /= (stem + objExt);

        std::error_code EC;
        {
            llvm::ToolOutputFile outFile(objPath.string(), EC, llvm::sys::fs::OF_None);
            if (EC) { std::cerr << "Could not create object file: " << EC.message() << "\n"; return 6; }

            llvm::legacy::PassManager pass;
            if (targetMachine->addPassesToEmitFile(pass, outFile.os(), nullptr, llvm::CodeGenFileType::ObjectFile)) {
                std::cerr << "TargetMachine can't emit object file\n"; return 7;
            }
            pass.run(*module);
            outFile.keep();
        }

        // try linkers in order
        int rc = -1;
        std::string cmd;

        rc = std::system((std::string("clang --version >nul 2>&1")).c_str());
        if (rc == 0) {
            cmd = "clang -o " + std::string("\"") + outp.string() + "\" " + std::string("\"") + objPath.string() + "\"";
            rc = std::system(cmd.c_str());
            if (rc == 0) { std::cout << "Wrote " << outp.string() << "\n"; return 0; }
        }

        rc = std::system((std::string("gcc --version >nul 2>&1")).c_str());
        if (rc == 0) {
            cmd = "gcc -o " + std::string("\"") + outp.string() + "\" " + std::string("\"") + objPath.string() + "\"";
            rc = std::system(cmd.c_str());
            if (rc == 0) { std::cout << "Wrote " << outp.string() << "\n"; return 0; }
        }

        rc = std::system((std::string("cl /? >nul 2>&1")).c_str());
        if (rc == 0) {
            cmd = std::string("cl /nologo ") + std::string("\"") + objPath.string() + std::string("\"") + std::string(" /Fe:") + std::string("\"") + outp.string() + std::string("\"");
            rc = std::system(cmd.c_str());
            if (rc == 0) { std::cout << "Wrote " << outp.string() << "\n"; return 0; }
        }

        std::cerr << "No usable linker found (tried clang, gcc, cl), or linking failed. You can link manually:\n";
        std::cerr << "  clang -o \"" << outp.string() << "\" \"" << objPath.string() << "\"\n";
        return 4;
    }

    // if not the simple print form, emit IR to stdout
    llvm::LLVMContext ctx;
    auto module = std::make_unique<llvm::Module>("oong_module", ctx);
    llvm::IRBuilder<> builder(ctx);
    std::vector<llvm::Type*> printf_arg_types;
    printf_arg_types.push_back(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx)));
    auto printf_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), printf_arg_types, true);
    auto printf_func = llvm::Function::Create(printf_type, llvm::Function::ExternalLinkage, "printf", module.get());
    auto main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), false);
    auto main_func = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", module.get());
    auto bb = llvm::BasicBlock::Create(ctx, "entry", main_func);
    builder.SetInsertPoint(bb);
    auto format_str = builder.CreateGlobalStringPtr("Hello from oong: %d\n");
    auto const_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 42);
    builder.CreateCall(printf_func, {format_str, const_val});
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
    if (llvm::verifyModule(*module, &llvm::errs())) { std::cerr << "Generated module is broken\n"; return 1; }
    module->print(llvm::outs(), nullptr);
    return 0;
}
