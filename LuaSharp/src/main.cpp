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

bool ReportDiagnostics(const std::filesystem::path& input, const luasharp::Analysis& analysis) {
    bool failed = false;
    for (const auto& diagnostic : analysis.diagnostics) {
        const bool error = diagnostic.severity == luasharp::DiagnosticSeverity::Error;
        std::cerr << input.string() << '(' << diagnostic.line + 1 << ',' << diagnostic.character + 1 << "): " << (error ? "error: " : "warning: ") << diagnostic.message << '\n';
        failed = failed || error;
    }
    return failed;
}

struct RunOptions {
    std::filesystem::path input;
    std::filesystem::path output;
    bool execute{true};
    bool debugHooks{};
    bool debug{}; // serve DAP and wait for a client before the module runs
    uint16_t port{4711};
};

RunOptions ParseRunOptions(int argc, char** argv, int start, bool execute) {
    if (argc <= start) throw std::runtime_error("Expected <file.ls>");
    RunOptions options;
    options.input = argv[start];
    options.execute = execute;
    for (int index = start + 1; index < argc; ++index) {
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
    if (options.output.empty()) options.output = options.input.parent_path() / (options.input.stem().string() + ".c");
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
    const auto& input = run.input;
    const auto source = ReadSource(input);
    // A standalone run has no host to give the declared functions.
    static const std::vector<luasharp::ExternalType> none;
    const auto& types = run.execute ? none : ProjectTypes(input);
    const auto analysis = luasharp::Analyze(source, types);
    if (ReportDiagnostics(input, analysis)) return 1;
    // A client sets breakpoints by full path.
    const auto sourceName = run.debug ? std::filesystem::absolute(input).string() : input.filename().string();
    auto generated = luasharp::CompileToC(analysis, types, {run.debugHooks, source, sourceName});
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
        std::cout << "\nRan " << input.string() << '\n';
    }
    return 0;
}

void Usage() {
    std::cerr << "Usage:\n"
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
            bool failed = false;
            for (int index = 2; index < argc; ++index) {
                const std::filesystem::path input = argv[index];
                try { failed = ReportDiagnostics(input, luasharp::Analyze(ReadSource(input), ProjectTypes(input))) || failed; }
                catch (const std::exception& error) { std::cerr << "luasharp: " << error.what() << '\n'; failed = true; }
            }
            return failed ? 1 : 0;
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
