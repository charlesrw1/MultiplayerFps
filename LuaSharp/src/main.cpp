#include "Compiler.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
void RunTests() {
    const auto lua = luasharp::Compile(R"(class Thing { private: Entity value = null; public: Thing() {} int get(Entity e) { if (e != null) { return e.blah(); } return 0; } } auto t = new Thing();)");
    assert(lua.find("---@field private value Entity?") != std::string::npos);
    assert(lua.find("function Thing:get(e)") != std::string::npos);
    assert(lua.find("return e:blah()") != std::string::npos);
    assert(lua.find("local t = Thing.new()") != std::string::npos);
    std::cout << "Tests passed\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test") { RunTests(); return 0; }
        if (argc != 2) { std::cerr << "Usage: luasharp <file.ls>\n"; return 2; }
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
