#include "Compiler.h"
#include "CBackend.h"
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

std::filesystem::path OutputArgument(int argc, char** argv, int start, const std::filesystem::path& fallback) {
    if (argc == start) return fallback;
    if (argc == start + 2 && (std::string_view(argv[start]) == "-o" || std::string_view(argv[start]) == "--output")) return argv[start + 1];
    throw std::runtime_error("Expected -o <output-file>");
}

bool IsDebugFlag(int argc, char** argv, int index) { return argc > index && std::string_view(argv[index]) == "--debug-hooks"; }

int CompileNative(const std::filesystem::path& input, const std::filesystem::path& output, bool execute, bool debugHooks, std::chrono::steady_clock::time_point cliStart) {
    using Clock = std::chrono::steady_clock;
    const auto milliseconds = [](Clock::time_point from, Clock::time_point to) { return std::chrono::duration<double, std::milli>(to - from).count(); };
    const auto source = ReadSource(input);
    // A standalone run has no host to give the declared functions.
    static const std::vector<luasharp::ExternalType> none;
    const auto& types = execute ? none : ProjectTypes(input);
    const auto analysis = luasharp::Analyze(source, types);
    if (ReportDiagnostics(input, analysis)) return 1;
    auto generated = luasharp::CompileToC(analysis, types, {debugHooks, source, input.filename().string()});
    const auto frontendEnd = Clock::now();
    WriteOutput(output, generated.c);
    const auto writeEnd = Clock::now();
    std::cout << "Wrote " << output.string() << '\n';
    if (execute) {
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
            const std::filesystem::path input = argv[2];
            const bool debugHooks = IsDebugFlag(argc, argv, 3);
            const auto output = OutputArgument(argc, argv, debugHooks ? 4 : 3, input.parent_path() / (input.stem().string() + ".c"));
            return CompileNative(input, output, command != "emit-c", debugHooks, cliStart);
        }
        const std::filesystem::path input = argv[1];
        const bool debugHooks = IsDebugFlag(argc, argv, 2);
        const auto output = OutputArgument(argc, argv, debugHooks ? 3 : 2, input.parent_path() / (input.stem().string() + ".c"));
        return CompileNative(input, output, true, debugHooks, cliStart);
    } catch (const std::exception& error) {
        std::cerr << "luasharp: " << error.what() << '\n';
        return 1;
    }
}
