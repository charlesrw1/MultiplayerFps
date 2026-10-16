#include "Compiler.h"
#include "CBackend.h"
#include "Semantic.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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

int CompileNative(const std::filesystem::path& input, const std::filesystem::path& output, bool execute, std::chrono::steady_clock::time_point cliStart) {
    const auto source = ReadSource(input);
    const auto analysis = luasharp::Analyze(source);
    if (ReportDiagnostics(input, analysis)) return 1;
    auto generated = luasharp::CompileToC(source);
    WriteOutput(output, generated.c);
    std::cout << "Wrote " << output.string() << '\n';
    if (execute) {
        luasharp::NativeCompileOptions options; options.initialize = false;
        auto module = luasharp::NativeModule::Load(std::move(generated), options);
        const std::chrono::duration<double, std::milli> compileTime = std::chrono::steady_clock::now() - cliStart;
        std::cout << "Compiled in " << std::fixed << std::setprecision(1) << compileTime.count() << " ms" << std::endl;
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
              << "  luasharp emit-lua <file.ls> [-o output.lua]\n"
              << "  luasharp check <file.ls> [more files]\n";
}

void RunTests() {
    const auto lua = luasharp::Compile(R"(class Value {} class Thing { private: Value value = null; public: Thing() {} int get(Value v) { if (v != null) { return v.read(); } return 0; } } auto t = new Thing();)");
    assert(lua.find("---@field private value Value?") != std::string::npos);
    assert(lua.find("function Thing:get(v)") != std::string::npos);
    assert(lua.find("return v:read()") != std::string::npos);
    assert(lua.find("local t = Thing.new()") != std::string::npos);
    const auto mapped = luasharp::CompileWithSourceMap("auto value = 1;");
    assert(mapped.sourceMap.size() == 1 && mapped.sourceMap.front().generatedLine == 1);
    std::cout << "Tests passed\n";
}
}

int main(int argc, char** argv) {
    const auto cliStart = std::chrono::steady_clock::now();
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test") { RunTests(); return 0; }
        if (argc < 2) { Usage(); return 2; }
        if (std::string_view(argv[1]) == "check") {
            if (argc < 3) { Usage(); return 2; }
            bool failed = false;
            for (int index = 2; index < argc; ++index) {
                const std::filesystem::path input = argv[index];
                try { failed = ReportDiagnostics(input, luasharp::Analyze(ReadSource(input))) || failed; }
                catch (const std::exception& error) { std::cerr << "luasharp: " << error.what() << '\n'; failed = true; }
            }
            return failed ? 1 : 0;
        }
        const auto command = std::string_view(argv[1]);
        if (command == "emit-c" || command == "run-native" || command == "run") {
            if (argc < 3) { Usage(); return 2; }
            const std::filesystem::path input = argv[2];
            const auto output = OutputArgument(argc, argv, 3, input.parent_path() / (input.stem().string() + ".c"));
            return CompileNative(input, output, command != "emit-c", cliStart);
        }
        if (command == "emit-lua") {
            if (argc < 3) { Usage(); return 2; }
            const std::filesystem::path input = argv[2]; const auto source = ReadSource(input); const auto analysis = luasharp::Analyze(source);
            if (ReportDiagnostics(input, analysis)) return 1;
            const auto output = OutputArgument(argc, argv, 3, input.parent_path() / (input.stem().string() + ".lua"));
            WriteOutput(output, luasharp::Compile(source)); std::cout << "Wrote " << output.string() << '\n'; return 0;
        }
        const std::filesystem::path input = argv[1];
        const auto output = OutputArgument(argc, argv, 2, input.parent_path() / (input.stem().string() + ".c"));
        return CompileNative(input, output, true, cliStart);
    } catch (const std::exception& error) {
        std::cerr << "luasharp: " << error.what() << '\n';
        return 1;
    }
}
