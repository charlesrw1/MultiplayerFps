#include "Compiler.h"
#include "LspBridge.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void Require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void TestTopLevelConstructorCall() {
    Require(luasharp::Compile("auto m = new MyClass();") == "local m = MyClass.new()\n", "new expression was not lowered to .new()");
}

void TestClassEmission() {
    const auto lua = luasharp::Compile(R"(class Player {
private:
Entity target = null;
table data = {};
public:
Player() {}
int score(Entity entity) {
    if (entity != null) { return entity.getScore(); }
    return 0;
}
})");
    Require(lua.find("---@field private target Entity?") != std::string::npos, "nullable field annotation missing");
    Require(lua.find("self.data = {}") != std::string::npos, "table field initializer missing");
    Require(lua.find("function Player:score(entity)") != std::string::npos, "method declaration missing");
    Require(lua.find("if entity~=nil then") != std::string::npos, "if condition was not lowered to Lua");
    Require(lua.find("return entity:getScore()") != std::string::npos, "method call was not lowered to colon syntax");
}

void TestStatementSourceMap() {
    constexpr std::string_view source = "auto first = 1;\nauto second = 2;";
    const auto result = luasharp::CompileWithSourceMap(source);
    Require(result.sourceMap.size() == 2, "each source statement must have a mapping");
    Require(result.sourceMap[0].generatedLine == 1 && result.sourceMap[0].sourceOffset == 0, "first mapping is incorrect");
    Require(result.sourceMap[1].generatedLine == 2 && result.sourceMap[1].sourceOffset == 16, "second mapping is incorrect");
}

void TestInvalidSyntax() {
    try {
        static_cast<void>(luasharp::Compile("class Broken { int field }"));
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("invalid class field did not produce a compiler error");
}

void TestLspPositionTranslation() {
    const auto document = luasharp::CreateVirtualDocument("file:///test.ls", "auto first = 1;\nauto second = 2;");
    Require(luasharp::ToGeneratedPosition(document, {1, 4}).line == 1, "source position did not map to generated line");
    Require(luasharp::ToSourcePosition(document, {1, 0}).line == 1, "generated position did not map back to source line");
}
}

int main() {
    const std::vector<std::pair<std::string_view, std::function<void()>>> tests{
        {"top-level constructor call", TestTopLevelConstructorCall},
        {"class emission", TestClassEmission},
        {"statement source map", TestStatementSourceMap},
        {"invalid syntax", TestInvalidSyntax},
        {"LSP position translation", TestLspPositionTranslation},
    };

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            return 1;
        }
    }
}
