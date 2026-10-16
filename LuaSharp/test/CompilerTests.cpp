#include "Compiler.h"
#include "LspBridge.h"
#include "Semantic.h"

#include <algorithm>
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

void TestSemanticAnalysis() {
    const auto analysis = luasharp::Analyze(R"(class Component {
int id = 1;
}
class Player : Component {
int health = 1;
public:
int score(Entity target) { return 0; }
}
auto player = new Player();
int invalid = "text";
)");
    const auto members = analysis.members.find("Player");
    Require(members != analysis.members.end() && members->second.size() == 3, "class and inherited members were not collected");
    Require(analysis.variableTypes.at("player") == "Player", "auto new type was not inferred");
    Require(!analysis.diagnostics.empty(), "type mismatch was not diagnosed");
}

void TestSemanticDiagnosticsAndParameters() {
    const auto analysis = luasharp::Analyze(R"(class Player {
void move(int speed, float seconds) {}
}

UnknownType value;
auto player = new Player();
player.missing();
)");
    const auto move = std::find_if(analysis.members.at("Player").begin(), analysis.members.at("Player").end(), [](const auto& symbol) { return symbol.name == "move"; });
    Require(move != analysis.members.at("Player").end() && move->parameters.size() == 2, "method parameter information was not retained");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& d) { return d.message.find("Unknown type") != std::string::npos; }), "unknown type was not diagnosed");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& d) { return d.message.find("Unknown member") != std::string::npos && d.severity == luasharp::DiagnosticSeverity::Warning; }), "unknown member was not warned");
}

void TestSharedSyntaxTree() {
    const auto syntax = luasharp::ParseSyntax(R"(class Child : Component {
int health = 10;
void move(int speed) { int distance = speed; }
}
auto child = new Child();
)");
    Require(syntax.classes.size() == 1 && syntax.classes[0].baseName == "Component", "class inheritance was not parsed into syntax tree");
    Require(syntax.classes[0].fields.size() == 1 && syntax.classes[0].functions.size() == 1, "class members were not parsed into syntax tree");
    Require(syntax.classes[0].functions[0].parameters[0].name == "speed", "function parameter was not parsed into syntax tree");
    Require(syntax.classes[0].functions[0].body[0].type == "int", "typed local was not parsed into syntax tree");
}
}

int main() {
    const std::vector<std::pair<std::string_view, std::function<void()>>> tests{
        {"top-level constructor call", TestTopLevelConstructorCall},
        {"class emission", TestClassEmission},
        {"statement source map", TestStatementSourceMap},
        {"invalid syntax", TestInvalidSyntax},
        {"LSP position translation", TestLspPositionTranslation},
        {"semantic analysis", TestSemanticAnalysis},
        {"semantic diagnostics and parameters", TestSemanticDiagnosticsAndParameters},
        {"shared syntax tree", TestSharedSyntaxTree},
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
