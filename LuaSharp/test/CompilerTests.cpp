#include "Compiler.h"
#include "CBackend.h"
#include "LspBridge.h"
#include "Semantic.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void Require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

long long NativeScale(void*, long long value) { return value * 3; }

const std::string& VariableType(const luasharp::Analysis& analysis, std::string_view name) {
    const auto found = analysis.variableTypes.find(name);
    Require(found != analysis.variableTypes.end(), "variable was not bound");
    return analysis.types.Name(found->second);
}

void TestTopLevelConstructorCall() {
    const auto lua = luasharp::Compile("class MyClass {}\nauto m = new MyClass();");
    Require(lua.find("local m = MyClass.new()\n") != std::string::npos, "new expression was not lowered to .new()");
}

void TestClassEmission() {
    const auto lua = luasharp::Compile(R"(class Entity { int getScore() { return 0; } }
class Player {
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
    Require(VariableType(analysis, "player") == "Player", "auto new type was not inferred");
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
    const auto* move = luasharp::FindMember(analysis, "Player", "move");
    Require(move && move->parameters.size() == 2, "method parameter information was not retained");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& d) { return d.message.find("Unknown type") != std::string::npos; }), "unknown type was not diagnosed");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& d) { return d.message.find("Unknown member") != std::string::npos && d.severity == luasharp::DiagnosticSeverity::Warning; }), "unknown member was not warned");
}

void TestSharedSyntaxTree() {
    const auto tree = luasharp::ParseSyntax(R"(class Child : Component {
int health = 10;
void move(int speed) { int distance = speed; }
}

auto child = new Child();
)");
    const auto& syntax = tree.root;
    Require(syntax.classes.size() == 1 && syntax.classes[0].baseName == "Component", "class inheritance was not parsed into syntax tree");
    Require(syntax.classes[0].fields.size() == 1 && syntax.classes[0].functions.size() == 1, "class members were not parsed into syntax tree");
    Require(syntax.classes[0].functions[0].parameters[0].name == "speed", "function parameter was not parsed into syntax tree");
    Require(tree.Statements(syntax.classes[0].functions[0].body)[0].type == "int", "typed local was not parsed into syntax tree");
    const auto topLevel = tree.Statements(syntax.topLevel);
    Require(topLevel.size() == 1 && tree.Expression(topLevel[0].expression).kind == luasharp::SyntaxExpression::Kind::New, "top-level statement was not linked into the syntax tree");
}

void TestExpressionTree() {
    const std::vector<luasharp::SyntaxToken> tokens{
        {luasharp::SyntaxTokenKind::Identifier, "player", {0, 6}}, {luasharp::SyntaxTokenKind::Symbol, ".", {6, 1}},
        {luasharp::SyntaxTokenKind::Identifier, "move", {7, 4}}, {luasharp::SyntaxTokenKind::Symbol, "(", {11, 1}},
        {luasharp::SyntaxTokenKind::Number, "1", {12, 1}}, {luasharp::SyntaxTokenKind::Symbol, "+", {14, 1}},
        {luasharp::SyntaxTokenKind::Number, "2", {16, 1}}, {luasharp::SyntaxTokenKind::Symbol, ")", {17, 1}},
        {luasharp::SyntaxTokenKind::Symbol, "[", {18, 1}}, {luasharp::SyntaxTokenKind::Number, "0", {19, 1}},
        {luasharp::SyntaxTokenKind::Symbol, "]", {20, 1}}};
    const auto parsed = luasharp::ParseExpression(tokens);
    const auto& tree = parsed.tree;
    const auto& expression = parsed.Root();
    Require(expression.kind == luasharp::SyntaxExpression::Kind::Index, "index expression was not parsed");
    const auto& call = tree.Child(expression, 0);
    Require(call.kind == luasharp::SyntaxExpression::Kind::Call, "call expression was not parsed");
    Require(tree.Child(call, 0).kind == luasharp::SyntaxExpression::Kind::Member, "member expression was not parsed");
    Require(tree.Child(call, 1).kind == luasharp::SyntaxExpression::Kind::Binary, "binary precedence expression was not parsed");
    Require(tree.Children(tree.Child(call, 1)).size() == 2 && tree.Child(tree.Child(call, 1), 1).text == "2", "binary operands were not linked in order");
}

void TestRecoveringSyntaxTree() {
    const auto tree = luasharp::ParseTree("class Broken { int value = ; auto x = 1;");
    Require(!tree.diagnostics.empty(), "malformed input did not retain syntax diagnostics");
    Require(tree.root.classes.size() == 1, "recovering parser discarded the class");
    Require(tree.tokens.back().kind == luasharp::SyntaxTokenKind::EndOfFile, "token stream has no EOF token");
    Require(std::any_of(tree.diagnostics.begin(), tree.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.span.offset > 0; }), "syntax diagnostics lost source spans");
}

void TestLexerTriviaAndSpans() {
    const auto bom = luasharp::ParseSyntax("\xEF\xBB\xBFint x = 1;");
    Require(bom.diagnostics.empty() && bom.tokens.front().span.offset == 3, "lexer did not skip a UTF-8 byte order mark");
    const auto tree = luasharp::ParseTree("/* header */\nclass Thing { // field\n int value = 12; }");
    Require(tree.diagnostics.empty(), "comments caused lexer diagnostics");
    Require(tree.root.classes[0].span.offset == 19, "class name span is incorrect");
    Require(tree.Expression(tree.root.classes[0].fields[0].initializer).kind == luasharp::SyntaxExpression::Kind::Number, "field initializer was not parsed");
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
    Require(VariableType(analysis, "damage") == "int", "member chain type was not inferred");
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

void TestListSemanticsAndLowering() {
    constexpr std::string_view source = R"(class Item {}
Item[] items = {};
items.push_back(new Item());
int count = items.size();
Item first = items[0];
items[-1] = first;
Item[] tail = items[1:3];
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid list operations produced diagnostics");
    Require(VariableType(analysis, "first") == "Item", "list indexing did not return the element type");
    Require(VariableType(analysis, "tail") == "Item[]", "list slicing did not preserve the list type");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("table.insert(items, Item.new())") != std::string::npos, "push_back was not lowered to table.insert");
    Require(lua.find("#(items)") != std::string::npos, "list size was not lowered");
    Require(lua.find("i + 1") != std::string::npos, "zero-based list indexing was not lowered");
    const auto invalid = luasharp::Analyze("int[] values = {};\nvalues.no_such_method();");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Unknown member 'no_such_method'") != std::string::npos; }), "unknown list method was not diagnosed");
}

void TestStringMethodsAndCasts() {
    constexpr std::string_view source = R"(string text = "Hello";
int length = text.length();
bool contains = text.contains("ell");
string part = text.substring(1, 3);
int whole = int(2.8);
float decimal = float(whole);
bool enabled = bool(whole);
string printed = string(enabled);
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid string methods or casts produced diagnostics");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("string.find(text, \"ell\", 1, true) ~= nil") != std::string::npos, "string contains was not lowered");
    Require(lua.find("math.floor") != std::string::npos, "float-to-int cast was not lowered");
    Require(lua.find("whole ~= 0") != std::string::npos, "number-to-bool cast was not lowered");
    Require(lua.find("tostring(enabled)") != std::string::npos, "string cast was not lowered");
}

void TestDynamicTableSemantics() {
    const auto analysis = luasharp::Analyze(R"(table values = {};
values["name"] = "thing";
values[4] = true;
auto first = values["name"];
auto arbitrary = values.any_key;
)");
    Require(analysis.diagnostics.empty(), "table rejected heterogeneous keys or values");
    Require(VariableType(analysis, "first") == "any" && VariableType(analysis, "arbitrary") == "any", "table reads did not produce dynamic values");
}

void TestNativeMathTypesAndOperators() {
    constexpr std::string_view source = R"(Vec3 a = Vec3.new(1, 2, 3);
Vec3 b = Vec3.splat(2);
Vec3 sum = a + b;
float dot = a.dot(b);
Quat rotation = Quat.from_euler(a);
Vec3 rotated = rotation * sum;
Transform transform = Transform.from_pos_rot(a, rotation);
Vec3 point = transform * rotated;
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid native math operations produced diagnostics");
    Require(VariableType(analysis, "rotated") == "Vec3" && VariableType(analysis, "point") == "Vec3", "native operator result types were wrong");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("Vec3.new(1, 2, 3)") != std::string::npos && lua.find("Vec3:new") == std::string::npos, "native static call used method-call syntax");
    Require(lua.find("rotation * sum") != std::string::npos && lua.find("transform * rotated") != std::string::npos, "native operators were not preserved for Lua metatables");
    const auto invalid = luasharp::Analyze("Quat q = Quat.identity();\nfloat bad = q + q;");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Operator '+' is not defined") != std::string::npos; }), "invalid native operator was not diagnosed");
}

void TestNullAndOperatorDiagnostics() {
    const auto valid = luasharp::Analyze("class Item {}\nItem item = null;\nbool missing = item == null;");
    Require(valid.diagnostics.empty(), "class value could not be compared or assigned to null");
    const auto invalid = luasharp::Analyze("int value = null;\nbool missing = value != null;");
    Require(std::count_if(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.severity == luasharp::DiagnosticSeverity::Error; }) >= 2, "null was accepted for a primitive value");
}

void TestBoundTreeElseIfAndImplicitMembers() {
    constexpr std::string_view source = R"(class Example {
int value = 0;
void helper() {}
int classify(int input) {
    if (input == 1) { return 10; }
    else if (input == 2) { return 20; }
    else { return 30; }
}
void update() { value = value + 1; helper(); }
})";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid else-if or implicit members produced diagnostics");
    Require(analysis.bound.classes.size() == 1 && analysis.bound.classes[0].functions.size() == 3, "bound semantic class tree was not built");
    const auto& program = analysis.bound;
    Require(program.Statements(program.Statements(program.classes[0].functions[1].body)[0].elseBody)[0].kind == luasharp::SyntaxStatement::Kind::If, "else-if chain was not retained in the bound tree");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("elseif input == 2 then") != std::string::npos, "else-if was not lowered to Lua elseif");
    Require(lua.find("self.value = self.value + 1") != std::string::npos, "implicit field access was not lowered through self");
    Require(lua.find("self:helper()") != std::string::npos, "implicit method call was not lowered through self");
}

void TestCoreSemanticGuards() {
    const auto analysis = luasharp::Analyze(R"(class Invalid {
void run() {
    if (1) {}
    1 = 2;
    int value = 1;
    value();
}
})");
    const auto has = [&](std::string_view text) { return std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [&](const auto& diagnostic) { return diagnostic.message.find(text) != std::string::npos; }); };
    Require(has("If condition must be 'bool'"), "non-boolean if condition was accepted");
    Require(has("Invalid assignment target"), "non-lvalue assignment target was accepted");
    Require(has("is not callable"), "non-callable value invocation was accepted");
    try { static_cast<void>(luasharp::Compile("int value = \"wrong\";")); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("transpilation did not stop on semantic errors");
}

void TestClassContracts() {
    const auto access = luasharp::Analyze(R"(class Base { private: int secret = 1; }
class Child : Base { int reveal() { return secret; } }
auto item = new Base();
int leaked = item.secret;
)");
    Require(std::count_if(access.diagnostics.begin(), access.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("is private") != std::string::npos; }) == 2, "private access was not enforced for derived and external callers");

    const auto overrideMismatch = luasharp::Analyze(R"(class Base { int read(int value) { return value; } }
class Child : Base { string read(int value) { return "bad"; } }
)");
    Require(std::any_of(overrideMismatch.diagnostics.begin(), overrideMismatch.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("does not match the inherited signature") != std::string::npos; }), "incompatible override was accepted");

    const auto lua = luasharp::Compile(R"(class Base { int inherited = 1; }
class Child : Base {}
auto child = new Child();
)");
    Require(lua.find("local self = setmetatable(Base.new(), Child)") != std::string::npos, "derived constructor did not initialize its base class");

    const auto requiredBase = luasharp::Analyze("class Base { Base(int value) {} } class Child : Base {}");
    Require(std::any_of(requiredBase.diagnostics.begin(), requiredBase.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("requires constructor arguments") != std::string::npos; }), "unsupported required base constructor was silently ignored");
}

void TestTypedDefaultsAndNullability() {
    const auto lua = luasharp::Compile(R"(class Reference {}
class Defaults {
int count;
float ratio;
bool enabled;
string name;
table values;
int[] items;
Vec3 position;
Reference reference;
void locals() { int localCount; string localName; Reference localReference; }
}
)");
    for (const auto& expected : {"self.count = 0", "self.ratio = 0.0", "self.enabled = false", "self.name = \"\"", "self.values = {}", "self.items = {}", "self.position = Vec3.new()", "self.reference = nil", "local localCount = 0", "local localName = \"\"", "local localReference = nil"})
        Require(lua.find(expected) != std::string::npos, std::string("missing typed default: ") + expected);
    const auto invalid = luasharp::Analyze("string text = null; Vec3 point = null;");
    Require(std::count_if(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Cannot assign") != std::string::npos; }) == 2, "null was accepted for non-null built-in value types");
}

void TestLoopsAndPopulatedLists() {
    constexpr std::string_view source = R"(int[] values = {1, 2, 3};
int index = 0;
while (index < values.size()) {
    index += 1;
}
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid while loop or populated list produced diagnostics");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("local values = {1, 2, 3}") != std::string::npos, "populated list literal was not emitted");
    Require(lua.find("while index < #(values) do") != std::string::npos, "while loop was not lowered");
    const auto invalid = luasharp::Analyze("string[] values = {\"ok\", 2}; while (1) {}");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("List element 2 expects 'string'") != std::string::npos; }), "list element type mismatch was not diagnosed");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("While condition must be 'bool'") != std::string::npos; }), "non-boolean while condition was accepted");
}

void TestCompoundAssignmentsAndFlowWarnings() {
    const auto analysis = luasharp::Analyze(R"(class Flow {
int run() { return 1; int unreachable = 2; }
}
string text = "a";
text += "b";
Vec3 vector = Vec3.new();
vector *= 2;
)");
    Require(std::none_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.severity == luasharp::DiagnosticSeverity::Error; }), "valid compound assignment produced an error");
    Require(std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.severity == luasharp::DiagnosticSeverity::Warning && diagnostic.message.find("Unreachable code") != std::string::npos; }), "unreachable statement was not warned");
    const auto lua = luasharp::Compile(R"(string text = "a"; text += "b"; Vec3 vector = Vec3.new(); vector *= 2;)");
    Require(lua.find("text = text .. \"b\"") != std::string::npos, "string compound concatenation used numeric addition");
    Require(lua.find("vector = vector * 2") != std::string::npos, "native compound operator was not emitted");
}

void TestKeyedTableLiterals() {
    constexpr std::string_view source = R"(table value = {name = "thing"; ["count"] = 2, [1 + 2] = true, 4};)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid keyed table literal produced diagnostics");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("{name = \"thing\", [\"count\"] = 2, [1 + 2] = true, 4}") != std::string::npos, "keyed table entries were not emitted as Lua fields");

    const auto invalid = luasharp::Analyze("int[] values = {first = 1};");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("cannot contain keyed entries") != std::string::npos; }), "keyed entry was accepted in a typed list literal");
}

void TestForBreakAndContinue() {
    constexpr std::string_view source = R"(int sum = 0;
for (int i = 0; i < 3; i += 1) {
    if (i == 1) { continue; }
    sum += i;
}
while (true) { break; }
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid for/break/continue statements produced diagnostics");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("local i = 0") != std::string::npos && lua.find("while i < 3 do") != std::string::npos, "for loop was not lowered");
    Require(lua.find("goto __luasharp_continue_") != std::string::npos, "continue was not lowered to a scoped label");
    Require(lua.find("i = i + 1") != std::string::npos, "for increment was not emitted after the continue label");
    Require(lua.find("break") != std::string::npos, "break was not emitted");

    const auto outOfScope = luasharp::Analyze("for (int i = 0; i < 1; i += 1) {} i = 2;");
    Require(std::any_of(outOfScope.diagnostics.begin(), outOfScope.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Unknown identifier 'i'") != std::string::npos; }), "for initializer escaped its loop scope");
    const auto invalid = luasharp::Analyze("break; continue;");
    Require(std::count_if(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("inside a loop") != std::string::npos; }) == 2, "out-of-loop break/continue were not diagnosed");
}

void TestForEachAndRangeLoops() {
    constexpr std::string_view source = R"(int[] values = {1, 2, 3};
table entries = {one = 1, two = 2};
int total = 0;
for (value in values) { total += value; }
for (index, value in values) { total += index; total += value; }
for (key, value in entries) { key = key; value = value; }
for (i in 1..10) {
    if (i == 3) { continue; }
    if (i == 8) { break; }
    total += i;
}
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid list, table, or range loop produced diagnostics");
    const auto lua = luasharp::Compile(source);
    Require(lua.find("for _, value in ipairs(values) do") != std::string::npos, "list range loop did not use ipairs");
    Require(lua.find("in ipairs(values) do") != std::string::npos && lua.find("local index = __luasharp_index_") != std::string::npos, "indexed list loop did not expose a zero-based index");
    Require(lua.find("for key, value in pairs(entries) do") != std::string::npos, "table range loop did not use pairs");
    Require(lua.find("for i = 1, 10 do") != std::string::npos, "integer range loop was not emitted inclusively");
    Require(lua.find("goto __luasharp_continue_") != std::string::npos, "continue did not work in a range loop");

    const auto invalid = luasharp::Analyze(R"(int[] values = {};
table entries = {};
for (value in entries) {}
for (i in 1.5..3) {}
for (value in 4) {}
value = 1;
)");
    for (const auto message : {"Table loops require key and value", "Range bounds must be 'int'", "is not iterable", "Unknown identifier 'value'"}) {
        Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [&](const auto& diagnostic) { return diagnostic.message.find(message) != std::string::npos; }), std::string("missing foreach diagnostic: ") + message);
    }
}

void TestEnums() {
    constexpr std::string_view source = R"(enum Direction {
    North,
    East = 5;
    South,
    Reverse = -2
}
Direction direction = Direction.North;
Direction defaultDirection;
direction = Direction.South;
bool east = direction == Direction.East;
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid enum declaration or access produced diagnostics");
    Require(analysis.members.contains("$type:Direction") && analysis.members.at("$type:Direction").size() == 4, "enum members were not registered for completion");
    const auto lua = luasharp::Compile(source);
    for (const auto expected : {"---@enum Direction", "North = 0", "East = 5", "South = 6", "Reverse = -2", "local direction = Direction.North", "local defaultDirection = 0"})
        Require(lua.find(expected) != std::string::npos, std::string("missing enum lowering: ") + expected);

    const auto invalid = luasharp::Analyze(R"(enum Empty {}
enum Bad { Same, Same, Fraction = 1.5 }
Bad value = 1;
Bad missing = Bad.Nope;
Bad nilValue = null;
Bad constructed = new Bad();
)");
    for (const auto message : {"must declare at least one member", "already declared", "integer constants", "Cannot assign", "Unknown member", "cannot be constructed"})
        Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [&](const auto& diagnostic) { return diagnostic.message.find(message) != std::string::npos; }), std::string("missing enum diagnostic: ") + message);
}

void TestLambdas() {
    constexpr std::string_view source = R"(int threshold = 2;
auto even = n => n % 2 == 0;
auto getter = ent => ent.get_value();
auto distanceSquared = (x, y) => x * x + y * y;
auto captured = n => n > threshold;
auto classify = n => {
    string label = "small";
    if (n > 10) { return "large"; }
    return label;
};
auto noop = () => {
    int localValue = threshold;
};
table mytable2 = {};
auto func4 = () => {
    mytable2["abc"] = 0;
};
bool isEven = even(4);
string category = classify(12);
noop();
auto value = getter({});
auto distance = distanceSquared(3, 4);
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid lambdas, calls, or captures produced diagnostics");
    const auto evenType = luasharp::ResolveVariableType(analysis, "even", source.find("even(4)"));
    Require(evenType && evenType->find("$function:1:bool") == 0, "lambda return type was not inferred");
    const auto lua = luasharp::Compile(source);
    for (const auto expected : {"local even = function(n) return n % 2 == 0 end", "local getter = function(ent) return ent:get_value() end", "local distanceSquared = function(x, y) return x * x + y * y end", "local captured = function(n) return n > threshold end"})
        Require(lua.find(expected) != std::string::npos, std::string("missing lambda lowering: ") + expected);
    Require(lua.find("local classify = function(n)\n") != std::string::npos && lua.find("return \"large\"") != std::string::npos && lua.find("return label") != std::string::npos, "multiline lambda body was not emitted");
    Require(lua.find("local noop = function()\n") != std::string::npos, "empty-return block lambda was not emitted");
    Require(lua.find("local func4 = function()\n    mytable2[\"abc\"] = 0\nend") != std::string::npos, "captured table assignment in a void block lambda was not emitted");

    const auto invalid = luasharp::Analyze(R"(auto pair = (x, x) => x;
pair(1);
pair(1, 2, 3);
auto inconsistent = n => { if (true) { return 1; } return "bad"; };
auto incomplete = n => { if (true) { return 1; } };
auto mixed = n => { if (true) { return; } return 1; };
auto empty = () => {};
auto invalidVoid = empty();
)" );
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("already declared") != std::string::npos; }), "duplicate lambda parameter was not diagnosed");
    Require(std::count_if(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("argument(s) for lambda") != std::string::npos; }) == 2, "lambda arity mismatches were not diagnosed");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("return types are inconsistent") != std::string::npos; }), "inconsistent block-lambda returns were not diagnosed");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("Not all lambda code paths") != std::string::npos; }), "missing block-lambda return path was not diagnosed");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("mix value and valueless") != std::string::npos; }), "mixed block-lambda returns were not diagnosed");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("void expression") != std::string::npos; }), "void lambda result was accepted as an auto initializer");
}

void TestCBackendEmission() {
    constexpr std::string_view source = R"(enum Mode { Off, On = 4 }
class Calculator {
    int base = 3;
    Calculator(int initial) { base = initial; }
    int add(int value) {
        if (value < 0) { return base; }
        int total = base;
        for (int i = 0; i < value; i += 1) { total += 1; }
        return total;
    }
    bool has(string text, string part) { return text.contains(part); }
}
)";
    const auto generated = luasharp::CompileToC(source);
    for (const auto expected : {
        "#define LS_Mode_On ((ls_int)4)",
        "struct LS_Calculator",
        "LS_Calculator* ls_Calculator_new(ls_int initial)",
        "ls_int ls_Calculator_add(LS_Calculator* self, ls_int value)",
        "self->base = initial",
        "while ((i < value))",
        "ls_string_contains(text, part)",
        "int ls_module_init(void)"})
        Require(generated.c.find(expected) != std::string::npos, std::string("missing native C lowering: ") + expected);
    Require(std::any_of(generated.exports.begin(), generated.exports.end(), [](const auto& value) { return value.symbolName == "ls_Calculator_add" && value.returnType == "int"; }), "native method export metadata is missing");

    const auto lists = luasharp::CompileToC("int[] values = {1, 2}; int[][] grid = {{1}, {}};");
    Require(lists.c.find("ls_list_create(sizeof(ls_int), LS_KIND_INT, 2)") != std::string::npos && lists.c.find("ls_list_create(sizeof(LS_List*), LS_KIND_LIST, 2)") != std::string::npos, "list literals were not typed from their declarations");

    const auto rejects = [](std::string_view source, std::string_view message) {
        try { static_cast<void>(luasharp::CompileToC(source)); }
        catch (const std::runtime_error& error) {
            Require(std::string_view(error.what()).find(message) != std::string_view::npos, std::string("unclear native error: ") + error.what());
            return;
        }
        throw std::runtime_error("native backend silently accepted: " + std::string(source));
    };
    rejects("class Base {} class Derived : Base {}", "does not support class inheritance");
    rejects("table t = {}; int[] values = t;", "cannot convert between 'table' and 'int[]'");
    rejects("int[] values = {}; table t = values;", "cannot convert between 'table' and 'int[]'");
}

void TestLibTccIntegrationWhenAvailable() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto module = luasharp::NativeModule::Compile(R"(class Calculator {
        int base = 1;
        Calculator(int initial) { base = initial; }
        int add(int value) { return base + value; }
    })");
    using Create = void* (*)(long long);
    using Add = long long (*)(void*, long long);
    const auto create = reinterpret_cast<Create>(module.FindSymbol("ls_Calculator_new"));
    const auto add = reinterpret_cast<Add>(module.FindSymbol("ls_Calculator_add"));
    Require(create && add, "libtcc module symbols were not exported");
    Require(add(create(7), 5) == 12, "libtcc-compiled LuaSharp method returned the wrong result");
}

void TestLibTccErrorsDoNotCrashHost() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    for (int attempt = 0; attempt < 2; ++attempt) {
        luasharp::CCompilationResult invalid; invalid.c = "long long f(void) { return undefined_name; }";
        try { static_cast<void>(luasharp::NativeModule::Load(invalid, {})); Require(false, "invalid C compiled"); }
        catch (const std::runtime_error& error) {
            Require(std::string_view(error.what()).find("'undefined_name' undeclared") != std::string_view::npos, std::string("TinyCC error lost its message: ") + error.what());
        }
    }
    auto valid = luasharp::NativeModule::Compile("int answer = 42;");
    Require(*static_cast<long long*>(valid.FindGlobal("answer")) == 42, "libtcc was unusable after a compile error");
}

void TestNativeStateArithmeticAndHostSymbols() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;

    auto state = luasharp::NativeModule::Compile(R"(int counter = 40;
counter += 2;
float fraction = 5 / 2;
int remainder = -5 % 3;
int wrapped = 9223372036854775807 + 1;
)");
    const auto counter = static_cast<long long*>(state.FindGlobal("counter"));
    const auto fraction = static_cast<double*>(state.FindGlobal("fraction"));
    const auto remainder = static_cast<long long*>(state.FindGlobal("remainder"));
    const auto wrapped = static_cast<long long*>(state.FindGlobal("wrapped"));
    Require(counter && *counter == 42, "persistent native module integer state is wrong");
    Require(fraction && *fraction == 2.5, "native int division did not preserve LuaSharp's float result");
    Require(remainder && *remainder == 1, "native modulo does not match Lua semantics for negative values");
    Require(wrapped && *wrapped == (-9223372036854775807LL - 1), "native integer overflow did not wrap deterministically");
    Require(state.FindGlobal("missing") == nullptr, "missing native global unexpectedly resolved");

    luasharp::ExternalType engine; engine.name = "EngineApi";
    luasharp::Symbol scale; scale.name = "scale"; scale.type = "int"; scale.kind = luasharp::SymbolKind::Method; scale.parameters = {"int value"}; scale.owner = engine.name;
    engine.members.push_back(scale);
    luasharp::NativeCompileOptions options;
    options.externalTypes.push_back(engine);
    options.symbols.push_back({"ls_EngineApi_scale", reinterpret_cast<const void*>(&NativeScale)});
    auto external = luasharp::NativeModule::Compile(R"(class Runner {
        int invoke(EngineApi api, int value) { return api.scale(value); }
    })", options);
    using Create = void* (*)();
    using Invoke = long long (*)(void*, void*, long long);
    const auto create = reinterpret_cast<Create>(external.FindSymbol("ls_Runner_new"));
    const auto invoke = reinterpret_cast<Invoke>(external.FindSymbol("ls_Runner_invoke"));
    Require(create && invoke && invoke(create(), nullptr, 7) == 21, "registered native host symbol was not called correctly");

    auto errors = luasharp::NativeModule::Compile(R"(class Arithmetic {
        int remainder(int left, int right) { return left % right; }
    })");
    using Remainder = long long (*)(void*, long long, long long);
    const auto makeArithmetic = reinterpret_cast<Create>(errors.FindSymbol("ls_Arithmetic_new"));
    const auto remainderFunction = reinterpret_cast<Remainder>(errors.FindSymbol("ls_Arithmetic_remainder"));
    Require(makeArithmetic && remainderFunction && remainderFunction(makeArithmetic(), 8, 0) == 0, "remainder-by-zero fallback result is wrong");
    Require(errors.LastError() == "integer remainder by zero", "native runtime error was not reported");
    errors.ClearError();
    Require(errors.LastError().empty(), "native runtime error could not be cleared");

    try { static_cast<void>(luasharp::NativeModule::Compile("int invalid = 1 % 0;")); }
    catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find("integer remainder by zero") != std::string_view::npos, "module initialization runtime error lost its cause");
        return;
    }
    throw std::runtime_error("module initialization ignored a native runtime error");
}

void TestNativeReloadSlots() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto program = luasharp::ReloadableNativeProgram::Compile("class ReloadValue { int stored = 0; int read() { return 1; } }");
    auto createSlot = program.FindFunction("ReloadValue.new");
    auto readSlot = program.FindFunction("ReloadValue.read");
    Require(createSlot && readSlot && readSlot.ReturnType() == "int" && readSlot.ParameterTypes() == std::vector<std::string>{"ReloadValue"}, "initial reload slots have incorrect metadata");
    using Create = void* (*)();
    using Read = long long (*)(void*);
    auto create = reinterpret_cast<Create>(createSlot.Load());
    void* instance = create();
    Require(reinterpret_cast<Read>(readSlot.Load())(instance) == 1, "initial function slot called the wrong implementation");

    program.Reload("class ReloadValue { int stored = 0; int read() { return 2; } int added() { return 3; } }");
    Require(program.Generation() == 2, "native reload generation did not advance");
    Require(reinterpret_cast<Read>(readSlot.Load())(instance) == 2, "stable function slot did not switch implementations");
    Require(static_cast<bool>(program.FindFunction("ReloadValue.added")), "new export was not added during reload");

    bool rejectedLayout = false;
    try { program.Reload("class ReloadValue { string stored = \"\"; int read() { return 2; } int added() { return 3; } }"); }
    catch (const std::runtime_error& error) {
        rejectedLayout = std::string_view(error.what()).find("changed native layout") != std::string_view::npos;
    }
    Require(rejectedLayout && program.Generation() == 2 && reinterpret_cast<Read>(readSlot.Load())(instance) == 2, "layout-changing reload mutated the active program");

    try { program.Reload("class ReloadValue { int stored = 0; float read() { return 2.0; } int added() { return 3; } }"); }
    catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find("changed native signature") != std::string_view::npos, "signature-changing reload produced the wrong error");
        Require(program.Generation() == 2 && reinterpret_cast<Read>(readSlot.Load())(instance) == 2, "failed reload mutated the active program");
        return;
    }
    throw std::runtime_error("signature-changing native reload was accepted");
}

void TestNativeGarbageCollection() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    luasharp::NativeCompileOptions options;
    options.runtime = runtime;
    auto module = luasharp::NativeModule::Compile(R"(
class Leaf {}
class Holder { Leaf leaf = new Leaf(); }
Leaf rooted = new Leaf();
)", options);

    Require(runtime->AllocationCount() == 1, "module initialization did not allocate its rooted object");
    Require(runtime->Collect() == 0 && runtime->AllocationCount() == 1, "module object global was not traced as a GC root");

    using Create = void* (*)();
    const auto createHolder = reinterpret_cast<Create>(module.FindSymbol("ls_Holder_new"));
    Require(createHolder != nullptr, "GC test constructor was not exported");
    void* holder = createHolder();
    Require(runtime->AllocationCount() == 3, "nested managed objects were not allocated through NativeRuntime");
    runtime->Pin(holder);
    Require(runtime->Collect() == 0 && runtime->AllocationCount() == 3, "pinned object graph was not traced transitively");
    runtime->Unpin(holder);
    Require(runtime->Collect() == 2 && runtime->AllocationCount() == 1, "unreachable managed object graph was not reclaimed");
}

void TestNativeInternedStrings() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    struct NativeString { long long length; char data[1]; };
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(
string first = "shared";
string second = "shared";
bool same = first == second;
bool contains = first.contains("are");
)", options);
        const auto first = *static_cast<NativeString**>(module.FindGlobal("first"));
        const auto second = *static_cast<NativeString**>(module.FindGlobal("second"));
        Require(first && first == second, "equal native strings were not interned to one object");
        Require(first->length == 6 && std::string_view(first->data, 6) == "shared", "interned native string has the wrong layout or contents");
        Require(*static_cast<unsigned char*>(module.FindGlobal("same")) == 1, "interned string equality failed");
        Require(*static_cast<unsigned char*>(module.FindGlobal("contains")) == 1, "managed string method failed");
        Require(runtime->AllocationCount() == 2, "string interning did not deduplicate repeated literals");
        Require(runtime->Collect() == 1 && runtime->AllocationCount() == 1, "string GC did not preserve rooted globals and reclaim temporary literals");
    }
    Require(runtime->Collect() == 1 && runtime->AllocationCount() == 0, "unreferenced interned strings were not collectible");
}

void TestNativeTables() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    struct NativeString { long long length; char data[1]; };
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        constexpr std::string_view tableSource = R"(
class Item { int value = 9; }
table values = { name = "bob", ["score"] = 4, [2] = new Item() };
values["updated"] = "yes";
auto name = values["name"];
auto score = values["score"];
auto object = values[2];
)";
        auto module = luasharp::NativeModule::Compile(tableSource, options);
        const auto name = *static_cast<luasharp::NativeValue*>(module.FindGlobal("name"));
        const auto score = *static_cast<luasharp::NativeValue*>(module.FindGlobal("score"));
        const auto object = *static_cast<luasharp::NativeValue*>(module.FindGlobal("object"));
        Require(name.kind == luasharp::NativeValueKind::String && name.as.pointer, "table string value was not tagged correctly");
        const auto text = static_cast<NativeString*>(name.as.pointer);
        Require(text->length == 3 && std::string_view(text->data, 3) == "bob", "table returned the wrong string value");
        Require(score.kind == luasharp::NativeValueKind::Int && score.as.integer == 4, "table returned the wrong integer value");
        Require(object.kind == luasharp::NativeValueKind::Object && object.as.pointer, "table returned the wrong managed object value");
        Require(runtime->Collect() == 0, "rooted table did not trace its managed keys and values");
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "table object graph was not reclaimed after module unload");
}

void TestNativeClosures() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(
class Capture {
    int run() {
        int base = 5;
        auto get = () => base;
        return get();
    }
}
table values = {};
auto setter = () => { values["abc"] = 7; };
auto even = n => n % 2 == 0;
auto sumSquares = (x, y) => x * x + y * y;
setter();
auto result = values["abc"];
bool isEven = even(8);
auto squared = sumSquares(3, 4);
)", options);
        const auto result = *static_cast<luasharp::NativeValue*>(module.FindGlobal("result"));
        Require(result.kind == luasharp::NativeValueKind::Int && result.as.integer == 7, "native closure did not execute its body");
        Require(*static_cast<unsigned char*>(module.FindGlobal("isEven")) == 1, "parameterized native closure returned the wrong boolean");
        const auto squared = *static_cast<luasharp::NativeValue*>(module.FindGlobal("squared"));
        Require(squared.kind == luasharp::NativeValueKind::Int && squared.as.integer == 25, "dynamic closure arithmetic returned the wrong value");
        using Create = void* (*)();
        using Run = long long (*)(void*);
        const auto create = reinterpret_cast<Create>(module.FindSymbol("ls_Capture_new"));
        const auto run = reinterpret_cast<Run>(module.FindSymbol("ls_Capture_run"));
        Require(create && run && run(create()) == 5, "native closure did not retain its local capture");
        Require(runtime->Collect() == 3, "collection did not reclaim the temporary instance, closure, and capture environment");
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "native closure graph was not reclaimed after module unload");
}

struct NativeString { long long length; char data[1]; };

template <typename T> T NativeGlobal(const luasharp::NativeModule& module, std::string_view name) {
    const auto* address = static_cast<const T*>(module.FindGlobal(name));
    Require(address != nullptr, "missing native global " + std::string(name));
    return *address;
}
std::string_view NativeText(const luasharp::NativeModule& module, std::string_view name) {
    const auto* text = NativeGlobal<const NativeString*>(module, name);
    Require(text != nullptr, "null native string " + std::string(name));
    return {text->data, static_cast<size_t>(text->length)};
}
void RequireNativeError(std::string_view source, std::string_view message) {
    try { static_cast<void>(luasharp::NativeModule::Compile(source)); }
    catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(message) != std::string_view::npos, std::string("unexpected native error: ") + error.what());
        return;
    }
    throw std::runtime_error("native module did not report: " + std::string(message));
}

void TestNativeLists() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(
class Bag {
    int[] numbers = {1, 2, 3};
    string[] names;
    int total() { int sum = 0; for (n in numbers) { sum += n; } return sum; }
}
int[] values = {10, 20, 30};
values.push_back(40);
values[0] = 5;
values[-1] += 2;
int popped = values.pop_back();
int size = values.size();
int last = values[-1];
int[] middle = values[1:3];
int middleSize = middle.size();
int middleFirst = middle[0];
middle.clear();
bool cleared = middle.empty();
int[][] grid = {{1, 2}, {3}};
int gridValue = grid[1][0];
string[] words = {};
for (int i = 0; i < 100; i += 1) { words.push_back("w" + string(i)); }
Bag bag = new Bag();
int bagTotal = bag.total();
int defaultNames = bag.names.size();
float[] floats = {1, 2.5};
float floatSum = floats[0] + floats[1];
)", options);
        const auto* values = NativeGlobal<const luasharp::NativeList*>(module, "values");
        Require(values && values->count == 3 && values->elementSize == 8 && values->elementKind == luasharp::NativeElementKind::Int, "native int list has the wrong layout");
        const auto* numbers = reinterpret_cast<const long long*>(values->data);
        Require(numbers[0] == 5 && numbers[1] == 20 && numbers[2] == 30, "native list writes or push/pop were wrong");
        Require(NativeGlobal<long long>(module, "popped") == 42, "compound list assignment or pop_back was wrong");
        Require(NativeGlobal<long long>(module, "size") == 3 && NativeGlobal<long long>(module, "last") == 30, "list size or negative index was wrong");
        Require(NativeGlobal<long long>(module, "middleSize") == 2 && NativeGlobal<long long>(module, "middleFirst") == 20, "list slice was wrong");
        Require(NativeGlobal<unsigned char>(module, "cleared") == 1, "list clear was wrong");
        Require(NativeGlobal<long long>(module, "gridValue") == 3, "nested list literal was wrong");
        Require(NativeGlobal<long long>(module, "bagTotal") == 6 && NativeGlobal<long long>(module, "defaultNames") == 0, "list fields or their defaults were wrong");
        Require(NativeGlobal<double>(module, "floatSum") == 3.5, "float list was wrong");

        runtime->Collect();
        const auto* words = NativeGlobal<const luasharp::NativeList*>(module, "words");
        Require(words && words->count == 100 && words->elementKind == luasharp::NativeElementKind::String, "string list grew incorrectly");
        const auto* word = reinterpret_cast<NativeString* const*>(words->data)[99];
        Require(std::string_view(word->data, static_cast<size_t>(word->length)) == "w99", "collection freed strings that a rooted list holds");
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "list graph was not reclaimed after module unload");
    RequireNativeError("int[] values = {1}; int bad = values[5];", "list index out of range");
    RequireNativeError("int[] values = {}; int bad = values.pop_back();", "pop_back on an empty list");
}

void TestNativeStrings() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto module = luasharp::NativeModule::Compile(R"(
string a = "foo";
string joined = a + "bar";
bool interned = joined == "foobar";
string grown = "x";
grown += "yz";
string word = "hello";
string middle = word.substring(1, 3);
string tail = word.substring(2);
string clamped = word.substring(3, 50);
string upper = "MiXed".upper();
string lower = "MiXed".lower();
string fromInt = string(42);
string fromFloat = string(2.5);
string fromWhole = string(3.0);
string fromBool = string(true);
int parsed = int("123");
float parsedFloat = float(" 1.5 ");
int parsedHex = int("0x10");
)");
    Require(NativeText(module, "joined") == "foobar" && NativeGlobal<unsigned char>(module, "interned") == 1, "string concatenation was wrong or not interned");
    Require(NativeText(module, "grown") == "xyz", "string += was wrong");
    Require(NativeText(module, "middle") == "ell" && NativeText(module, "tail") == "llo" && NativeText(module, "clamped") == "lo", "substring did not match Lua string.sub");
    Require(NativeText(module, "upper") == "MIXED" && NativeText(module, "lower") == "mixed", "string case conversion was wrong");
    Require(NativeText(module, "fromInt") == "42" && NativeText(module, "fromFloat") == "2.5" && NativeText(module, "fromWhole") == "3.0" && NativeText(module, "fromBool") == "true", "string() did not match Lua tostring");
    Require(NativeGlobal<long long>(module, "parsed") == 123 && NativeGlobal<double>(module, "parsedFloat") == 1.5 && NativeGlobal<long long>(module, "parsedHex") == 16, "string to number was wrong");
    RequireNativeError("int bad = int(\"abc\");", "string is not a number");
}

void TestNativeLoops() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto module = luasharp::NativeModule::Compile(R"(
int rangeSum = 0;
for (i in 1..4) { rangeSum += i; }
int filtered = 0;
for (i in 1..10) { if (i % 2 == 0) { continue; } if (i > 7) { break; } filtered += i; }
int emptyRange = 0;
for (i in 3..1) { emptyRange += 1; }
int[] items = {4, 5, 6};
int indexSum = 0;
int valueSum = 0;
for (index, item in items) { indexSum += index; valueSum += item; }
int nestedSum = 0;
for (item in items) { for (int j = 0; j < 2; j += 1) { if (j == 1) { continue; } nestedSum += item; } }
table scores = { a = 1, b = 2 };
int keyCount = 0;
bool foundTwo = false;
for (key, score in scores) { keyCount += 1; if (score == 2) { foundTwo = true; } }
)");
    Require(NativeGlobal<long long>(module, "rangeSum") == 10 && NativeGlobal<long long>(module, "emptyRange") == 0, "range loop was wrong");
    Require(NativeGlobal<long long>(module, "filtered") == 16, "continue or break in a range loop was wrong");
    Require(NativeGlobal<long long>(module, "indexSum") == 3 && NativeGlobal<long long>(module, "valueSum") == 15, "index/value list loop was wrong");
    Require(NativeGlobal<long long>(module, "nestedSum") == 15, "loop nested in a list loop was wrong");
    Require(NativeGlobal<long long>(module, "keyCount") == 2 && NativeGlobal<unsigned char>(module, "foundTwo") == 1, "table loop was wrong");
}

void TestNativeDynamicTables() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(
class Enemy {
    int hp = 10;
    string name = "orc";
    int hit(int amount) { hp -= amount; return hp; }
}
table t = {a = 1, b = "x", nested = {deep = 5}};
class Probe { int read(table source) { return int(source.nested.deep); } }
t["a"] += 2;
t.b += "y";
int a = t["a"];
string b = t.b;
t.nested.deep = 7;
int deep = t["nested"]["deep"];
t[1.0] = "one";
string one = t[1];
table fresh;
fresh.k = 3;
int k = int(fresh.k);
t.a = null;
bool removed = t.a == null;
bool missingFalsy = false;
if (!t.missing) { missingFalsy = true; }
t.add = (self, x) => x + 1;
int added = t.add(41);
table holder = {enemy = new Enemy()};
int hp = holder.enemy.hit(3);
holder.enemy.hp = 100;
Enemy direct = holder.enemy;
int directHp = direct.hp;
int[] xs = {1, 2, 3};
table lists = {xs = xs};
int listSum = 0;
for (v in lists.xs) { listSum += int(v); }
table outer = {seq = {10, 20, 30, null, 50}};
int seqSum = 0;
for (v in outer.seq) { seqSum += int(v); }
int keyCount = 0;
for (key, value in t) { keyCount += 1; }
table big = {};
for (int i = 0; i < 1000; i += 1) { big[i] = i * 2; }
int bigValue = big[999];
)", options);
        runtime->Collect();
        using Create = void* (*)();
        using Read = long long (*)(void*, void*);
        const auto create = reinterpret_cast<Create>(module.FindSymbol("ls_Probe_new"));
        const auto read = reinterpret_cast<Read>(module.FindSymbol("ls_Probe_read"));
        Require(create && read && read(create(), NativeGlobal<void*>(module, "t")) == 7 && module.LastError().empty(), "member names or table contents did not survive collection");
        Require(NativeGlobal<long long>(module, "a") == 3 && NativeText(module, "b") == "xy", "compound table assignment was wrong");
        Require(NativeGlobal<long long>(module, "deep") == 7, "nested member assignment or index was wrong");
        Require(NativeText(module, "one") == "one", "float key 1.0 and int key 1 were not the same key");
        Require(NativeGlobal<long long>(module, "k") == 3, "default table was not an empty table");
        Require(NativeGlobal<unsigned char>(module, "removed") == 1 && NativeGlobal<unsigned char>(module, "missingFalsy") == 1, "nil entries or truthiness were wrong");
        Require(NativeGlobal<long long>(module, "added") == 42, "table member call did not pass the table first");
        Require(NativeGlobal<long long>(module, "hp") == 7 && NativeGlobal<long long>(module, "directHp") == 100, "dynamic object method or field access was wrong");
        Require(NativeGlobal<long long>(module, "listSum") == 6 && NativeGlobal<long long>(module, "seqSum") == 60, "foreach over any was wrong");
        Require(NativeGlobal<long long>(module, "keyCount") == 4, "table iteration did not skip removed entries");
        Require(NativeGlobal<long long>(module, "bigValue") == 1998, "large table lookup was wrong");
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "dynamic table graph was not reclaimed after module unload");
    RequireNativeError("table t = {b = \"x\"}; int bad = t.b;", "expected int, got string");
    RequireNativeError("class A {} class B {} table t = {a = new A()}; B bad = t.a;", "expected B, got A");
    RequireNativeError("class A {} table t = {a = new A()}; auto bad = t.a.missing;", "no field 'missing' on A");
    RequireNativeError("table t = {}; t[null] = 1;", "table index is nil");
    RequireNativeError("table t = {}; auto bad = t.f(1);", "attempt to call a nil value");
}

void TestNativeClosureCaptures() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(
class Counter {
    int total = 0;
    int run(int start) {
        auto bump = () => { start += 1; total += start; };
        bump(); bump();
        return start;
    }
}
auto makeCounter = () => {
    int local = 0;
    return () => { local += 1; return local; };
};
auto next = makeCounter();
next(); next();
int third = next();
auto other = makeCounter();
int otherFirst = other();
auto makeAdder = (k) => (x) => x + k;
int added = makeAdder(5)(10);
auto pick = (v) => { if (v > 3) { return "big"; } return "small"; };
string big = pick(5);
string small = pick(1);
int[] seen = {};
for (i in 1..3) { auto f = () => i * 10; seen.push_back(f()); }
Counter counter = new Counter();
int start = counter.run(1);
int total = counter.total;
)", options);
        Require(NativeGlobal<long long>(module, "third") == 3 && NativeGlobal<long long>(module, "otherFirst") == 1, "closures did not share or isolate their captured cells");
        Require(NativeGlobal<long long>(module, "added") == 15, "nested closure did not capture through its parent");
        Require(NativeText(module, "big") == "big" && NativeText(module, "small") == "small", "block closure returned the wrong value");
        const auto* seen = NativeGlobal<const luasharp::NativeList*>(module, "seen");
        Require(seen && seen->count == 3 && reinterpret_cast<const long long*>(seen->data)[2] == 30, "loop closures captured the wrong values");
        Require(NativeGlobal<long long>(module, "start") == 3 && NativeGlobal<long long>(module, "total") == 5, "assigned parameter capture was not shared");
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "closure cells were not reclaimed after module unload");
}

void TestDynamicLuaEmission() {
    const auto lua = luasharp::Compile("table t = {}; t.n += 1; t.s += \"x\"; string[] words = {\"a\"}; words[0] += \"b\"; int n = int(t.n); bool b = bool(t.s);");
    for (const auto expected : {"t.n = t.n + 1", "t.s = t.s .. \"x\"", "a[i + 1] = a[i + 1] .. v", "local b = (not not t.s)"})
        Require(lua.find(expected) != std::string::npos, std::string("missing dynamic Lua lowering: ") + expected + "\n" + lua);
    const auto invalid = luasharp::Analyze("table t = {}; int n = 1; n /= t.x;");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& value) { return value.message.find("'/=' is 'float'") != std::string::npos; }), "int /= any was accepted");
}

void TestPrintfBuiltin() {
    constexpr std::string_view source = R"(printf("i=%d f=%f s=%s b=%b %%\n", -7, 2.5, "ok", true);)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid printf call produced diagnostics");
    Require(luasharp::Compile(source).find("io.write(string.format(") != std::string::npos, "printf was not lowered for Lua");
    const auto invalid = luasharp::Analyze("printf(1);");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& value) { return value.message.find("expects 'string'") != std::string::npos; }), "printf accepted a non-string format");
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    std::ostringstream output;
    auto* previous = std::cout.rdbuf(output.rdbuf());
    try {
        auto module = luasharp::NativeModule::Compile(source);
        auto lambda = luasharp::NativeModule::Compile(R"(auto say = () => printf("from lambda\n"); say();)");
    } catch (...) { std::cout.rdbuf(previous); throw; }
    std::cout.rdbuf(previous);
    Require(output.str() == "i=-7 f=2.5 s=ok b=true %\nfrom lambda\n", "native printf produced unexpected output: " + output.str());
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
        {"list semantics and lowering", TestListSemanticsAndLowering},
        {"string methods and casts", TestStringMethodsAndCasts},
        {"dynamic table semantics", TestDynamicTableSemantics},
        {"native math types and operators", TestNativeMathTypesAndOperators},
        {"null and operator diagnostics", TestNullAndOperatorDiagnostics},
        {"bound tree, else-if, and implicit members", TestBoundTreeElseIfAndImplicitMembers},
        {"core semantic guards", TestCoreSemanticGuards},
        {"class contracts", TestClassContracts},
        {"typed defaults and nullability", TestTypedDefaultsAndNullability},
        {"loops and populated lists", TestLoopsAndPopulatedLists},
        {"compound assignments and flow warnings", TestCompoundAssignmentsAndFlowWarnings},
        {"keyed table literals", TestKeyedTableLiterals},
        {"for, break, and continue", TestForBreakAndContinue},
        {"foreach and range loops", TestForEachAndRangeLoops},
        {"enums", TestEnums},
        {"lambdas", TestLambdas},
        {"dynamic Lua emission", TestDynamicLuaEmission},
        {"C backend emission", TestCBackendEmission},
        {"libtcc integration", TestLibTccIntegrationWhenAvailable},
        {"libtcc errors do not crash host", TestLibTccErrorsDoNotCrashHost},
        {"native lists", TestNativeLists},
        {"native strings", TestNativeStrings},
        {"native loops", TestNativeLoops},
        {"native dynamic tables", TestNativeDynamicTables},
        {"native closure captures", TestNativeClosureCaptures},
        {"native state, arithmetic, and host symbols", TestNativeStateArithmeticAndHostSymbols},
        {"native reload slots", TestNativeReloadSlots},
        {"native garbage collection", TestNativeGarbageCollection},
        {"native interned strings", TestNativeInternedStrings},
        {"native tables", TestNativeTables},
        {"native closures", TestNativeClosures},
        {"printf builtin", TestPrintfBuiltin},
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
