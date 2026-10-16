#include "Compiler.h"
#include "Semantic.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
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
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test") { RunTests(); return 0; }
        if (argc < 2) { std::cerr << "Usage: luasharp [check] <file.ls> [more files]\n"; return 2; }
        if (std::string_view(argv[1]) == "check") {
            if (argc < 3) { std::cerr << "Usage: luasharp check <file.ls> [more files]\n"; return 2; }
            bool failed = false;
            for (int index = 2; index < argc; ++index) {
                const std::filesystem::path input = argv[index]; std::ifstream sourceFile(input, std::ios::binary);
                if (!sourceFile) { std::cerr << "luasharp: Cannot open " << input.string() << '\n'; failed = true; continue; }
                std::ostringstream source; source << sourceFile.rdbuf();
                const auto analysis = luasharp::Analyze(source.str());
                for (const auto& diagnostic : analysis.diagnostics) {
                    const bool error = diagnostic.severity == luasharp::DiagnosticSeverity::Error;
                    std::cerr << input.string() << '(' << diagnostic.line + 1 << ',' << diagnostic.character + 1 << "): " << (error ? "error: " : "warning: ") << diagnostic.message << '\n';
                    failed = failed || error;
                }
            }
            return failed ? 1 : 0;
        }
        if (argc != 2) { std::cerr << "Usage: luasharp [check] <file.ls> [more files]\n"; return 2; }
        const std::filesystem::path input = argv[1];
        std::ifstream sourceFile(input, std::ios::binary);
        if (!sourceFile) throw std::runtime_error("Cannot open " + input.string());
        std::ostringstream source; source << sourceFile.rdbuf();
        const auto output = input.parent_path() / (input.stem().string() + ".lua");
        std::ofstream(output, std::ios::binary) << luasharp::Compile(source.str());
        std::cout << "Wrote " << output.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "luasharp: " << error.what() << '\n';
        return 1;
    }
}
