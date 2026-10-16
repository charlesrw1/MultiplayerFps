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
    Require(lua.find("if entity ~= nil then") != std::string::npos, "if condition was not lowered to Lua");
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

void TestExpressionTree() {
    const std::vector<luasharp::SyntaxToken> tokens{
        {luasharp::SyntaxTokenKind::Identifier, "player", {0, 6}}, {luasharp::SyntaxTokenKind::Symbol, ".", {6, 1}},
        {luasharp::SyntaxTokenKind::Identifier, "move", {7, 4}}, {luasharp::SyntaxTokenKind::Symbol, "(", {11, 1}},
        {luasharp::SyntaxTokenKind::Number, "1", {12, 1}}, {luasharp::SyntaxTokenKind::Symbol, "+", {14, 1}},
        {luasharp::SyntaxTokenKind::Number, "2", {16, 1}}, {luasharp::SyntaxTokenKind::Symbol, ")", {17, 1}},
        {luasharp::SyntaxTokenKind::Symbol, "[", {18, 1}}, {luasharp::SyntaxTokenKind::Number, "0", {19, 1}},
        {luasharp::SyntaxTokenKind::Symbol, "]", {20, 1}}};
    const auto expression = luasharp::ParseExpression(tokens);
    Require(expression.kind == luasharp::SyntaxExpression::Kind::Index, "index expression was not parsed");
    Require(expression.children[0].kind == luasharp::SyntaxExpression::Kind::Call, "call expression was not parsed");
    Require(expression.children[0].children[0].kind == luasharp::SyntaxExpression::Kind::Member, "member expression was not parsed");
    Require(expression.children[0].children[1].kind == luasharp::SyntaxExpression::Kind::Binary, "binary precedence expression was not parsed");
}

void TestRecoveringSyntaxTree() {
    const auto tree = luasharp::ParseTree("class Broken { int value = ; auto x = 1;");
    Require(!tree.diagnostics.empty(), "malformed input did not retain syntax diagnostics");
    Require(tree.root.classes.size() == 1, "recovering parser discarded the class");
    Require(tree.tokens.back().kind == luasharp::SyntaxTokenKind::EndOfFile, "token stream has no EOF token");
    Require(std::any_of(tree.diagnostics.begin(), tree.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.span.offset > 0; }), "syntax diagnostics lost source spans");
}

void TestLexerTriviaAndSpans() {
    const auto tree = luasharp::ParseTree("/* header */\nclass Thing { // field\n int value = 12; }");
    Require(tree.diagnostics.empty(), "comments caused lexer diagnostics");
    Require(tree.root.classes[0].span.offset == 19, "class name span is incorrect");
    Require(tree.root.classes[0].fields[0].initializer.kind == luasharp::SyntaxExpression::Kind::Number, "field initializer was not parsed");
    const auto broken = luasharp::ParseTree("/* never closed");
    Require(!broken.diagnostics.empty() && broken.diagnostics[0].message.find("Unterminated block comment") != std::string::npos, "unterminated comment was not diagnosed");
}

void TestScopeAwareBinding() {
    constexpr std::string_view source = R"(auto value = new Outer();
if (true) {
    auto value = new Inner();
    value.run();
}
value.run();
)";
    const auto analysis = luasharp::Analyze(source);
    const auto innerUse = source.find("value.run");
    const auto outerUse = source.rfind("value.run");
    Require(luasharp::ResolveVariableType(analysis, "value", innerUse) == "Inner", "inner shadow did not bind to inner variable");
    Require(luasharp::ResolveVariableType(analysis, "value", outerUse) == "Outer", "outer reference leaked inner scope");
}

void TestBoundMemberChain() {
    const auto analysis = luasharp::Analyze(R"(class Weapon { int damage = 1; }
class Player { Weapon weapon = new Weapon(); }
auto player = new Player();
auto damage = player.weapon.damage;
)" );
    Require(analysis.variableTypes.at("damage") == "int", "member chain type was not inferred");
    Require(analysis.diagnostics.empty(), "valid member chain produced diagnostics");
}

void TestSemanticErrors() {
    const auto analysis = luasharp::Analyze(R"(class Bad : Bad {
int duplicate;
int duplicate;
int get() { return "wrong"; }
})");
    const auto has = [&](std::string_view text) { return std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [&](const auto& diagnostic) { return diagnostic.message.find(text) != std::string::npos; }); };
    Require(has("already declared"), "duplicate member was not diagnosed");
    Require(has("Inheritance cycle"), "inheritance cycle was not diagnosed");
    Require(has("Cannot return"), "return type mismatch was not diagnosed");
}

void TestConstructorAndGroupingEmission() {
    const auto lua = luasharp::Compile(R"(class Point {
int x;
Point(int initial) { x = initial; }
}
auto point = new Point(2);
auto result = (1 + 2) * 3;
)");
    Require(lua.find("function Point.new(initial)") != std::string::npos, "constructor parameters were not emitted");
    Require(lua.find("Point.new(2)") != std::string::npos, "constructor arguments were not emitted");
    Require(lua.find("(1 + 2) * 3") != std::string::npos, "parenthesized expression semantics were lost");
}

void TestCallDiagnostics() {
    const auto analysis = luasharp::Analyze(R"(class Receiver {
void send(string message, int count) {}
}
auto receiver = new Receiver();
receiver.send(12);
)");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Expected 2 argument") != std::string::npos && diagnostic.severity == luasharp::DiagnosticSeverity::Error; }), "call arity error was not produced");
}

void TestReassignmentAndThis() {
    const auto analysis = luasharp::Analyze(R"(class Example {
string name;
void update() {
    int value = 1;
    value = "wrong";
    this.name = "right";
}
})");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Cannot assign 'string' to 'int'") != std::string::npos; }), "reassignment to a different type was not diagnosed");
    Require(std::none_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Unknown identifier 'this'") != std::string::npos; }), "this was not bound in a method");
    const auto lua = luasharp::Compile(R"(class Example { string name; void update() { this.name = "right"; } })");
    Require(lua.find("self.name = \"right\"") != std::string::npos, "this was not lowered to Lua self");
}

void TestUnknownNameAndMissingReturn() {
    const auto analysis = luasharp::Analyze(R"(class Example {
int calculate() { typo(); }
})");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Unknown identifier 'typo'") != std::string::npos; }), "unknown identifier was not diagnosed");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Not all code paths return") != std::string::npos; }), "missing return was not diagnosed");
}

void TestNoEngineTypesAreHardcoded() {
    const auto unknown = luasharp::Analyze("Entity value;");
    Require(std::any_of(unknown.diagnostics.begin(), unknown.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Unknown type 'Entity'") != std::string::npos; }), "engine type was incorrectly hardcoded as a language type");
    const auto declared = luasharp::Analyze("class Anything {}\nAnything value;");
    Require(declared.diagnostics.empty(), "user-declared type was not registered dynamically");
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
        {"expression tree", TestExpressionTree},
        {"recovering syntax tree", TestRecoveringSyntaxTree},
        {"lexer trivia and spans", TestLexerTriviaAndSpans},
        {"scope-aware binding", TestScopeAwareBinding},
        {"bound member chain", TestBoundMemberChain},
        {"semantic errors", TestSemanticErrors},
        {"constructor and grouping emission", TestConstructorAndGroupingEmission},
        {"call diagnostics", TestCallDiagnostics},
        {"reassignment and this", TestReassignmentAndThis},
        {"unknown name and missing return", TestUnknownNameAndMissingReturn},
        {"no hardcoded engine types", TestNoEngineTypesAreHardcoded},
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
