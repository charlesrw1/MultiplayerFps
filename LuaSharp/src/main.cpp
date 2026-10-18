#include "Compiler.h"
#include "CBackend.h"
#include "NativeDapServer.h"
#include "NativeDebugger.h"
#include "Project.h"
#include "Semantic.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace {
std::string ReadSource(const std::filesystem::path& input) {
    std::ifstream file(input, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open " + input.string());
    std::ostringstream source; source << file.rdbuf(); return source.str();
}

void WriteOutput(const std::filesystem::path& output, std::string_view contents) {
    std::ofstream file(output, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write " + output.string());
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!file) throw std::runtime_error("Failed while writing " + output.string());
}

// The host types declared by the luasharp.json project of input, if any. Cached per project.
const std::vector<luasharp::ExternalType>& ProjectTypes(const std::filesystem::path& input) {
    static std::map<std::string, std::vector<luasharp::ExternalType>> cache;
    static const std::vector<luasharp::ExternalType> none;
    const auto projectFile = luasharp::FindProjectFile(input);
    if (!projectFile) return none;
    const auto key = luasharp::PathKey(*projectFile);
    if (const auto found = cache.find(key); found != cache.end()) return found->second;
    const auto project = luasharp::LoadProject(*projectFile);
    for (const auto& entry : project.missing)
        std::cerr << projectFile->string() << ": warning: declaration path '" << entry << "' does not exist\n";
    std::vector<std::string> sources;
    for (const auto& file : project.declarationFiles) sources.push_back(ReadSource(file));
    return cache.emplace(key, luasharp::DeclarationTypes(sources)).first->second;
}

// The host types of the files, which must belong to one luasharp.json project (or none).
const std::vector<luasharp::ExternalType>& ProgramTypes(const std::vector<std::filesystem::path>& inputs) {
    const auto key = [](const std::filesystem::path& input) { const auto file = luasharp::FindProjectFile(input); return file ? luasharp::PathKey(*file) : std::string{}; };
    for (const auto& input : inputs)
        if (key(input) != key(inputs.front())) throw std::runtime_error(input.string() + " is not in the luasharp.json project of " + inputs.front().string());
    return ProjectTypes(inputs.front());
}

// The program's files; each name is how diagnostics and the debugger show it.
struct Program {
    std::vector<std::string> texts;
    std::vector<luasharp::SourceFile> files;
};
Program ReadProgram(const std::vector<std::filesystem::path>& inputs, bool fullPaths) {
    Program program;
    for (const auto& input : inputs) program.texts.push_back(ReadSource(input));
    for (size_t i{}; i < inputs.size(); ++i)
        program.files.push_back({fullPaths ? std::filesystem::absolute(inputs[i]).string() : inputs[i].string(), program.texts[i]});
    return program;
}

bool ReportDiagnostics(const luasharp::Analysis& analysis) {
    bool failed = false;
    for (const auto& diagnostic : analysis.diagnostics) {
        const bool error = diagnostic.severity == luasharp::DiagnosticSeverity::Error;
        std::cerr << analysis.sources.Name(diagnostic.file) << '(' << diagnostic.line + 1 << ',' << diagnostic.character + 1 << "): " << (error ? "error: " : "warning: ") << diagnostic.message << '\n';
        failed = failed || error;
    }
    return failed;
}

struct RunOptions {
    std::vector<std::filesystem::path> inputs; // one program
    std::filesystem::path output;
    bool execute{true};
    bool debugHooks{};
    bool debug{}; // serve DAP and wait for a client before the module runs
    uint16_t port{4711};
};

RunOptions ParseRunOptions(int argc, char** argv, int start, bool execute) {
    RunOptions options;
    options.execute = execute;
    int index = start;
    for (; index < argc && !std::string_view(argv[index]).starts_with('-'); ++index) options.inputs.emplace_back(argv[index]);
    if (options.inputs.empty()) throw std::runtime_error("Expected <file.ls>");
    for (; index < argc; ++index) {
        const std::string_view argument = argv[index];
        const auto value = [&] {
            if (index + 1 >= argc) throw std::runtime_error("Expected a value after " + std::string(argument));
            return std::string(argv[++index]);
        };
        if (argument == "--debug-hooks") options.debugHooks = true;
        else if (argument == "--debug") options.debug = options.debugHooks = true;
        else if (argument == "--port") options.port = static_cast<uint16_t>(std::stoul(value()));
        else if (argument == "-o" || argument == "--output") options.output = value();
        else throw std::runtime_error("Unknown option " + std::string(argument));
    }
    if (options.debug && !execute) throw std::runtime_error("--debug needs run");
    const auto& first = options.inputs.front();
    if (options.output.empty()) options.output = first.parent_path() / (first.stem().string() + ".c");
    return options;
}

// Runs the module under a DAP server. The module starts after the client's configurationDone.
int RunDebug(luasharp::CCompilationResult generated, uint16_t port) {
    auto runtime = luasharp::NativeRuntime::Create();
    luasharp::NativeCompileOptions options;
    options.initialize = false;
    options.runtime = runtime;
    auto module = luasharp::NativeModule::Load(std::move(generated), options);
    luasharp::NativeDebugger debugger(runtime);
    debugger.AddModule(module);
    luasharp::NativeDapServer server(debugger, port);
    runtime->SetPrintHandler([&](std::string_view text) { std::cout << text << std::flush; server.SendOutput(text); });
    std::cout << "Waiting for a debugger on port " << server.Port() << std::endl;
    while (!server.WaitUntilConfigured(std::chrono::hours(1))) {}
    int status = 0;
    try { module.Initialize(); }
    catch (const std::exception& error) {
        std::cerr << "luasharp: " << error.what() << '\n';
        server.SendOutput(std::string(error.what()) + '\n', "stderr");
        status = 1;
    }
    runtime->SetPrintHandler({});
    server.SendTerminated();
    server.WaitUntilDisconnected(std::chrono::seconds(5));
    debugger.RemoveModule(module);
    return status;
}

int CompileNative(const RunOptions& run, std::chrono::steady_clock::time_point cliStart) {
    using Clock = std::chrono::steady_clock;
    const auto milliseconds = [](Clock::time_point from, Clock::time_point to) { return std::chrono::duration<double, std::milli>(to - from).count(); };
    // A client sets breakpoints by full path.
    const auto program = ReadProgram(run.inputs, run.debug);
    // A standalone run has no host to give the declared functions.
    static const std::vector<luasharp::ExternalType> none;
    const auto& types = run.execute ? none : ProgramTypes(run.inputs);
    const auto analysis = luasharp::Analyze(program.files, types);
    if (ReportDiagnostics(analysis)) return 1;
    auto generated = luasharp::CompileToC(analysis, types, {run.debugHooks});
    const auto frontendEnd = Clock::now();
    WriteOutput(run.output, generated.c);
    const auto writeEnd = Clock::now();
    std::cout << "Wrote " << run.output.string() << '\n';
    if (run.debug) return RunDebug(std::move(generated), run.port);
    if (run.execute) {
        luasharp::NativeCompileOptions options; options.initialize = false;
        auto module = luasharp::NativeModule::Load(std::move(generated), options);
        const auto tccEnd = Clock::now();
        std::cout << std::fixed << std::setprecision(1) << "Compiled in " << milliseconds(cliStart, tccEnd) << " ms (frontend "
                  << milliseconds(cliStart, frontendEnd) << " ms, write C " << milliseconds(frontendEnd, writeEnd) << " ms, tcc "
                  << milliseconds(writeEnd, tccEnd) << " ms)" << std::endl;
        module.Initialize();
        std::cout << "\nRan " << run.inputs.front().string() << (run.inputs.size() > 1 ? " and the other files" : "") << '\n';
    }
    return 0;
}

void Usage() {
    std::cerr << "Usage:\n"
              << "  Several .ls files compile as one program: each sees the types of the others.\n"
              << "  luasharp <file.ls> [-o generated.c]       Emit C, compile, and run\n"
              << "  luasharp run <file.ls> [-o generated.c]   Emit C, compile, and run\n"
              << "  luasharp emit-c <file.ls> [-o output.c]   Emit C only\n"
              << "  luasharp run <file.ls> --debug [--port N] Serve the debugger (DAP, port 4711), then run\n"
              << "  --debug-hooks after <file.ls> emits an instrumented build for the debugger\n"
              << "  luasharp check <file.ls> [more files]\n"
              << "check and emit-c use the declarations of the nearest luasharp.json above each file.\n";
}
}

int main(int argc, char** argv) {
    const auto cliStart = std::chrono::steady_clock::now();
    try {
        if (argc < 2) { Usage(); return 2; }
        if (std::string_view(argv[1]) == "check") {
            if (argc < 3) { Usage(); return 2; }
            const std::vector<std::filesystem::path> inputs(argv + 2, argv + argc);
            const auto program = ReadProgram(inputs, false);
            return ReportDiagnostics(luasharp::Analyze(program.files, ProgramTypes(inputs))) ? 1 : 0;
        }
        const auto command = std::string_view(argv[1]);
        if (command == "emit-c" || command == "run-native" || command == "run") {
            if (argc < 3) { Usage(); return 2; }
            return CompileNative(ParseRunOptions(argc, argv, 2, command != "emit-c"), cliStart);
        }
        return CompileNative(ParseRunOptions(argc, argv, 1, true), cliStart);
    } catch (const std::exception& error) {
        std::cerr << "luasharp: " << error.what() << '\n';
        return 1;
    }
}
