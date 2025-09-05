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

int run_interpreter(const std::string &source)
{
    auto isNumber = [](const std::string &s)
    {
        if (s.empty())
            return false;
        size_t i = 0;
        if (s[0] == '-')
            i = 1;
        bool dot = false;
        for (; i < s.size(); ++i)
        {
            if (s[i] == '.')
            {
                if (dot)
                    return false;
                dot = true;
                continue;
            }
            if (!isdigit(s[i]))
                return false;
        }
        return true;
    };

    // Simple environment for variables (supports const obj = {...}; with string/number/nested)
    struct Value
    {
        enum Kind
        {
            NUMBER,
            STRING,
            OBJECT,
            BOOL
        } kind;
        double num = 0;
        std::string str;
        std::map<std::string, Value> obj;
        bool b = false;
        Value() : kind(STRING), num(0) {}
        Value(double n) : kind(NUMBER), num(n) {}
        Value(const std::string &s) : kind(STRING), str(s) {}
        Value(const std::map<std::string, Value> &o) : kind(OBJECT), obj(o) {}
        Value(bool bv) : kind(BOOL), b(bv) {}
    };
    std::map<std::string, Value> env;

    // Helper: skip whitespace
    auto skip_ws = [](const std::string &s, size_t &i)
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
            ++i;
    };
    // Recursive object literal parser
    std::function<Value(const std::string &, size_t &)> parse_object;
    std::function<Value(const std::string &, size_t &)> parse_value;
    parse_value = [&](const std::string &s, size_t &i) -> Value
    {
        skip_ws(s, i);
        if (i < s.size() && s[i] == '{')
        {
            return parse_object(s, i);
        }
        else if (i < s.size() && (s[i] == '"' || s[i] == '\''))
        {
            char quote = s[i++];
            std::string val;
            while (i < s.size() && s[i] != quote)
            {
                if (s[i] == '\\' && i + 1 < s.size())
                {
                    val += s[i + 1];
                    i += 2;
                }
                else
                    val += s[i++];
            }
            if (i < s.size() && s[i] == quote)
                ++i;
            return Value(val);
        }
        else
        {
            // Parse number, bool, or identifier (as string)
            size_t start = i;
            while (i < s.size() && (isalnum(s[i]) || s[i] == '.' || s[i] == '-'))
                ++i;
            std::string val = s.substr(start, i - start);
            if (val == "true")
                return Value(true);
            if (val == "false")
                return Value(false);
            if (!val.empty() && std::all_of(val.begin(), val.end(), [](char c)
                                            { return isdigit(c) || c == '.' || c == '-'; }))
            {
                return Value(std::stod(val));
            }
            return Value(val);
        }
    };
    parse_object = [&](const std::string &s, size_t &i) -> Value
    {
        std::map<std::string, Value> obj;
        if (i < s.size() && s[i] == '{')
            ++i;
        skip_ws(s, i);
        while (i < s.size() && s[i] != '}')
        {
            skip_ws(s, i);
            // Parse key
            std::string key;
            if (s[i] == '"' || s[i] == '\'')
            {
                char quote = s[i++];
                while (i < s.size() && s[i] != quote)
                {
                    if (s[i] == '\\' && i + 1 < s.size())
                    {
                        key += s[i + 1];
                        i += 2;
                    }
                    else
                        key += s[i++];
                }
                if (i < s.size() && s[i] == quote)
                    ++i;
            }
            else
            {
                size_t start = i;
                while (i < s.size() && (isalnum(s[i]) || s[i] == '_'))
                    ++i;
                key = s.substr(start, i - start);
            }
            skip_ws(s, i);
            if (i < s.size() && s[i] == ':')
                ++i;
            skip_ws(s, i);
            // Parse value
            Value val = parse_value(s, i);
            obj[key] = val;
            skip_ws(s, i);
            if (i < s.size() && s[i] == ',')
                ++i;
            skip_ws(s, i);
        }
        if (i < s.size() && s[i] == '}')
            ++i;
        return Value(obj);
    };

    std::string yellow = "\033[33m";
    std::string reset = "\033[0m";

    // Serialize Value to JS-like string, with optional color for the whole object
    std::function<std::string(const Value &, const std::string &)> serialize;
    serialize = [&](const Value &v, const std::string &objColor) -> std::string
    {
        if (v.kind == Value::NUMBER)
        {
            std::ostringstream oss;
            oss << yellow << v.num << reset;
            return oss.str();
        }
        else if (v.kind == Value::BOOL)
        {
            return yellow + (v.b ? std::string("true") : std::string("false")) + reset;
        }
        else if (v.kind == Value::STRING)
        {
            if (!objColor.empty())
                return objColor + v.str + reset;
            else
                return v.str;
        }
        else if (v.kind == Value::OBJECT)
        {
            std::string out = objColor + "{ ";
            bool first = true;
            for (const auto &kv : v.obj)
            {
                if (!first)
                    out += objColor + ", ";
                first = false;
                out += objColor + kv.first + reset + objColor + ": " + serialize(kv.second, objColor);
            }
            out += objColor + " }" + reset;
            return out;
        }
        return "<unknown>";
    };
    // parse source into AST
    Parser P(source);
    auto R = P.parse();
    if (!R.ok || !R.stmt)
    {
        std::cerr << "Interpreter Parse error: " << R.error << "\n";
        return 1;
    }

    // Initialize native target for JIT
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Create LLJIT
    auto JOrErr = llvm::orc::LLJITBuilder().create();
    if (!JOrErr)
    {
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
    // removed duplicate yellow and reset definitions
    auto *prog = dynamic_cast<Program *>(R.stmt.get());
    // Pass 1: collect const obj = {...} declarations (very simple, only top-level, only one object)
    if (prog)
    {
        // Pass 1: handle VarDeclStmt for const/var declarations
        for (const auto &s : prog->statements)
        {
            auto *var = dynamic_cast<VarDeclStmt *>(s.get());
            if (var && var->value)
            {
                if (auto lit = dynamic_cast<LiteralExpr *>(var->value.get()))
                {
                    std::string v = lit->value;
                    size_t i = 0;
                    skip_ws(v, i);
                    if (!v.empty() && i < v.size() && v[i] == '{')
                    {
                        Value obj = parse_object(v, i);
                        env[var->name] = obj;
                    }
                    else if (v == "true")
                    {
                        env[var->name] = Value(true);
                    }
                    else if (v == "false")
                    {
                        env[var->name] = Value(false);
                    }
                    else if (!v.empty() && std::all_of(v.begin(), v.end(), [](char c){ return isdigit(c) || c == '.' || c == '-'; }))
                    {
                        env[var->name] = Value(std::stod(v));
                    }
                    else
                    {
                        env[var->name] = Value(v);
                    }
                }
            }
        }
        for (const auto &s : prog->statements)
        {
            if (auto ps = dynamic_cast<PrintStmt *>(s.get()))
            {
                std::string color = "";
                if (ps->origin == TokenKind::Tok_ConsoleError)
                {
                    color = "\033[31m"; // red
                }
                else if (ps->origin == TokenKind::Tok_ConsoleWarn)
                {
                    color = "\033[38;2;255;165;0m"; // orange-yellow
                }
                else if (ps->origin == TokenKind::Tok_ConsoleInfo)
                {
                    color = "\033[34m"; // blue
                }
                else if (ps->origin == TokenKind::Tok_ConsoleSuccess)
                {
                    color = "\033[32m"; // green
                }
                std::string result;
                for (size_t i = 0; i < ps->args.size(); ++i)
                {
                    std::string argStr;
                    if (auto lit = dynamic_cast<LiteralExpr *>(ps->args[i].get()))
                    {
                        if (lit->value == "true") {
                            argStr = yellow + std::string("true") + reset;
                        } else if (lit->value == "false") {
                            argStr = yellow + std::string("false") + reset;
                        } else if (isNumber(lit->value)) {
                            argStr = yellow + lit->value + reset;
                        } else {
                            argStr = color + lit->value;
                        }
                    }
                    else if (auto id = dynamic_cast<IdentifierExpr *>(ps->args[i].get()))
                    {
                        // Look up in env
                        auto it = env.find(id->name);
                        if (it != env.end())
                        {
                            // If the value is BOOL, always output as yellow true/false
                            if (it->second.kind == Value::BOOL) {
                                argStr = yellow + (it->second.b ? std::string("true") : std::string("false")) + reset;
                            } else if (
                                ps->origin == TokenKind::Tok_ConsoleSuccess ||
                                ps->origin == TokenKind::Tok_ConsoleError ||
                                ps->origin == TokenKind::Tok_ConsoleWarn ||
                                ps->origin == TokenKind::Tok_ConsoleInfo ||
                                ps->origin == TokenKind::Tok_ConsoleLog
                            ) {
                                argStr = color + serialize(it->second, color);
                            } else {
                                argStr = color + serialize(it->second, "");
                            }
                        }
                        else
                        {
                            argStr =  color + "<undefined>";
                        }
                    }
                    else if (auto call = dynamic_cast<CallExpr *>(ps->args[i].get()))
                    {
                        argStr =  color + call->callee + "()";
                    }
                    else
                    {
                        argStr =  color + "<unsupported expr>";
                    }
                    if (i > 0)
                        result += " ";
                    result += argStr;
                }
                if (!color.empty())
                {
                    result = color + result + reset;
                }
                auto out_str = builder.CreateGlobalStringPtr(result.c_str());
                builder.CreateCall(puts_func, {out_str});
            }
        }
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
    }
    else if (auto ps = dynamic_cast<PrintStmt *>(R.stmt.get()))
    {
        std::string color = "";
        if (ps->origin == TokenKind::Tok_ConsoleError)
        {
            color = "\033[31m";
        }
        else if (ps->origin == TokenKind::Tok_ConsoleWarn)
        {
            color = "\033[38;2;255;165;0m";
        }
        else if (ps->origin == TokenKind::Tok_ConsoleInfo)
        {
            color = "\033[34m";
        }
        else if (ps->origin == TokenKind::Tok_ConsoleSuccess)
        {
            color = "\033[32m";
        }
        std::string result;
        for (size_t i = 0; i < ps->args.size(); ++i)
        {
            std::string argStr;
            if (auto lit = dynamic_cast<LiteralExpr *>(ps->args[i].get()))
            {
                if (lit->value == "true" || lit->value == "false")
                {
                    argStr = yellow + lit->value + reset;
                }
                else if (isNumber(lit->value) && ps->origin == TokenKind::Tok_Print)
                {
                    argStr = yellow + lit->value + reset;
                }
                else
                {
                    argStr = color + lit->value;
                }
            }
            else if (auto id = dynamic_cast<IdentifierExpr *>(ps->args[i].get()))
            {
                auto it = env.find(id->name);
                if (it != env.end())
                {
                    // If the value is BOOL, always output as yellow true/false
                    if (it->second.kind == Value::BOOL) {
                        argStr = yellow + (it->second.b ? std::string("true") : std::string("false")) + reset;
                    } else if (
                        ps->origin == TokenKind::Tok_ConsoleSuccess ||
                        ps->origin == TokenKind::Tok_ConsoleError ||
                        ps->origin == TokenKind::Tok_ConsoleWarn ||
                        ps->origin == TokenKind::Tok_ConsoleInfo ||
                        ps->origin == TokenKind::Tok_ConsoleLog
                    ) {
                        argStr = color + serialize(it->second, color);
                    } else {
                        argStr = color + serialize(it->second, "");
                    }
                }
                else
                {
                    argStr =  color + "<undefined>";
                }
            }
            else if (auto call = dynamic_cast<CallExpr *>(ps->args[i].get()))
            {
                argStr = color + call->callee + "()" + reset;
            }
            else
            {
                argStr = color + "<unsupported expr>" + reset;
            }
            if (i > 0)
                result += " ";
            result += argStr;
        }
        if (!color.empty())
        {
            result = color + result + reset;
        }
        auto out_str = builder.CreateGlobalStringPtr(result.c_str());
        builder.CreateCall(puts_func, {out_str});
        builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
    }
    else
    {
        std::cerr << "Unsupported statement\n";
        return 1;
    }

    if (llvm::verifyModule(*M, &llvm::errs()))
    {
        std::cerr << "Generated module is broken\n";
        return 3;
    }

    // Make module thread-safe and add to JIT
    llvm::orc::ThreadSafeModule TSM(std::move(M), std::move(TSCtx));
    // Add current process symbols so printf resolves to the C runtime
    if (auto GenOrErr = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(J->getDataLayout().getGlobalPrefix()))
    {
        auto Gen = std::move(*GenOrErr);
        J->getMainJITDylib().addGenerator(std::move(Gen));
    }
    else
    {
        llvm::logAllUnhandledErrors(GenOrErr.takeError(), llvm::errs(), "Failed to create DynamicLibrarySearchGenerator: ");
    }

    if (auto Err = J->addIRModule(std::move(TSM)))
    {
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
