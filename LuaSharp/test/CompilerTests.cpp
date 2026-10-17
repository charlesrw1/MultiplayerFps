#include "Compiler.h"
#include "CBackend.h"
#include "Semantic.h"
#include "Builtins.h"

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

void TestInvalidSyntax() {
    try {
        static_cast<void>(luasharp::ParseSyntax("class Broken { int field }"));
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("invalid class field did not produce a compiler error");
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
    const auto invalid = luasharp::Analyze("table t = {}; int n = 1; n /= t.x;");
    Require(std::any_of(invalid.diagnostics.begin(), invalid.diagnostics.end(), [](const auto& value) { return value.message.find("'/=' is 'float'") != std::string::npos; }), "int /= any was accepted");
}

void TestNativeMathTypesAndOperators() {
    constexpr std::string_view source = R"(Vec3 a = Vec3.new(1, 2, 3);
Vec3 b = Vec3.splat(2);
Vec3 sum = a + b;
float dot = a.dot(b);
Quat rotation = Quat.from_euler(a);
Vec3 rotated = rotation * sum;
Mat4x4 transform = Mat4x4.from_pos_rot(a, rotation);
Vec3 point = transform * rotated;
)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid native math operations produced diagnostics");
    Require(VariableType(analysis, "rotated") == "Vec3" && VariableType(analysis, "point") == "Vec3", "native operator result types were wrong");
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
    try { static_cast<void>(luasharp::CompileToC("int value = \"wrong\";")); }
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

    const auto requiredBase = luasharp::Analyze("class Base { Base(int value) {} } class Child : Base {}");
    Require(std::any_of(requiredBase.diagnostics.begin(), requiredBase.diagnostics.end(), [](const auto& diagnostic) { return diagnostic.message.find("has no parameterless constructor") != std::string::npos; }), "missing base constructor arguments were accepted");
}

void TestBuiltinNullability() {
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
}

void TestKeyedTableLiterals() {
    constexpr std::string_view source = R"(table value = {name = "thing"; ["count"] = 2, [1 + 2] = true, 4};)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid keyed table literal produced diagnostics");

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
    const auto generated = luasharp::CompileToC(source);
    for (const auto expected : {"#define LS_Direction_North ((ls_int)0)", "#define LS_Direction_East ((ls_int)5)", "#define LS_Direction_South ((ls_int)6)", "#define LS_Direction_Reverse ((ls_int)-2)"})
        Require(generated.c.find(expected) != std::string::npos, std::string("missing enum lowering: ") + expected);

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
    Require(lists.c.find("ls_list_create(sizeof(ls_int), LS_KIND_INT, (const LS_TypeInfo*)0, 2)") != std::string::npos && lists.c.find("ls_list_create(sizeof(LS_List*), LS_KIND_LIST, &ls_listtype_") != std::string::npos, "list literals were not typed from their declarations");

    const auto rejects = [](std::string_view source, std::string_view message) {
        try { static_cast<void>(luasharp::CompileToC(source)); }
        catch (const std::runtime_error& error) {
            Require(std::string_view(error.what()).find(message) != std::string_view::npos, std::string("unclear native error: ") + error.what());
            return;
        }
        throw std::runtime_error("native backend silently accepted: " + std::string(source));
    };
    try {
        static_cast<void>(luasharp::CompileToC("class Derived : Component {}", {{"Component", "", {}, false, false}}));
        throw std::runtime_error("native backend silently accepted a class deriving from an external type");
    } catch (const std::runtime_error& error) { Require(std::string_view(error.what()).find("from external type 'Component'") != std::string_view::npos, std::string("unclear native error: ") + error.what()); }
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

void TestPrintfBuiltin() {
    constexpr std::string_view source = R"(printf("i=%d f=%f s=%s b=%b %%\n", -7, 2.5, "ok", true);)";
    const auto analysis = luasharp::Analyze(source);
    Require(analysis.diagnostics.empty(), "valid printf call produced diagnostics");
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

bool HasDiagnostic(const luasharp::Analysis& analysis, std::string_view text) {
    return std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [&](const auto& diagnostic) { return diagnostic.message.find(text) != std::string::npos; });
}

void RequireClean(std::string_view source, std::string_view message) {
    const auto analysis = luasharp::Analyze(source);
    std::string detail;
    for (const auto& diagnostic : analysis.diagnostics) if (diagnostic.severity == luasharp::DiagnosticSeverity::Error) detail += " [" + diagnostic.message + "]";
    Require(detail.empty(), std::string(message) + detail);
}

void RequireDiagnostic(std::string_view source, std::string_view expected) {
    const auto analysis = luasharp::Analyze(source);
    Require(HasDiagnostic(analysis, expected), "expected diagnostic '" + std::string(expected) + "' for: " + std::string(source));
}

void TestStructSemantics() {
    RequireClean(R"(struct Pair {
    int a = 1;
    float b;
    Vec3 where;
    Pair(int x) { a = x; }
    int sum() { return a; }
    static Pair make() { return Pair(3); }
}
Pair p = Pair(2);
Pair q = new Pair(5);
Pair d = Pair();
Pair[] many = {Pair(1), Pair(2)};
many[0].a = 7;
p.where.x = 4;
int total = many[1].sum() + Pair.make().sum();
bool same = p == q;)", "valid struct program");
    RequireClean("class Node {} struct Refs { Node n; string s = \"x\"; int[] list; table t; Refs[] children; static Refs empty; }", "struct reference fields");
    RequireDiagnostic("struct A { B inner; } struct B { A outer; }", "cannot contain itself");
    RequireDiagnostic("struct Bad { Bad again; }", "cannot contain itself");
    RequireDiagnostic("struct Base {} struct Derived : Base {}", "Structs cannot have a base type");
    RequireDiagnostic("struct P { int a; } class F { static P make() { return P(); } } F.make().a = 3;", "temporary struct");
    RequireDiagnostic("struct P { int a; } P p = null;", "Cannot assign");
    RequireDiagnostic("struct P { int a; } bool b = P() == null;", "Cannot compare 'null'");
    RequireDiagnostic("class C {} C c = C();", "Use 'new'");
    RequireDiagnostic("struct Vec3 { int a; }", "built-in type");
    RequireDiagnostic("struct S { int a; void f() { auto g = () => a; } }", "Lambdas inside struct methods");
}

void TestStaticFieldSemantics() {
    RequireClean(R"(class Counter {
    static int count = 0;
    static string label = "n" + string(count);
    static int[] history;
    int id;
    Counter() { count += 1; id = count; history.push_back(id); }
    static int next() { return count + 1; }
}
struct Grid { int w; static Grid unit = Grid(); static int made; }
Counter a = new Counter();
Counter.count = 5;
Counter.count += 1;
int n = Counter.count + Counter.next();
Grid.unit.w = 3;
Grid.made += 1;)", "valid static field program");
    RequireDiagnostic("class A { int x; static int y = x; }", "Instance member 'x' cannot be used in a static method");
    RequireDiagnostic("class A { static int y = this; }", "'this' cannot be used in a static method");
    RequireDiagnostic("class A { private: static int hidden; } int v = A.hidden;", "is private");
    RequireDiagnostic("class A { static int count; } A a = new A(); int v = a.count;", "Unknown member 'count'");
    RequireDiagnostic("class A { static string count = 1; }", "Cannot assign");
    const auto analysis = luasharp::Analyze("class A { static int count; int f() { return count; } }\nint v = A.count;");
    const auto staticFields = std::count_if(analysis.bound.expressions.begin(), analysis.bound.expressions.end(), [](const auto& item) { return item.lowering == luasharp::Lowering::StaticField; });
    Require(staticFields == 2, "implicit and qualified static field reads were not lowered as static fields");
}

void TestInheritanceSemantics() {
    RequireClean(R"(abstract class Shape {
    string name;
    Shape(string n) { name = n; }
    abstract float area();
    virtual string describe() { return name + " " + string(area()); }
    int sides() { return 0; }
}
class Rect : Shape {
    float w; float h;
    Rect(float w, float h) : base("rect") { this.w = w; this.h = h; }
    override float area() { return w * h; }
    override string describe() { return "[" + base.describe() + "]"; }
}
sealed class Square : Rect {
    Square(float s) : base(s, s) {}
    sealed override float area() { return base.area(); }
}
class Plain { int base = 1; int f() { return base; } }
Shape s = new Square(2);
bool isRect = s is Rect;
Rect r = s as Rect;
table t = {v = s};
bool anyIsSquare = t.v is Square;
bool anyIsInt = t.v is int;
Rect fromAny = t.v as Rect;
int sides = r.sides();)", "valid inheritance program");
    RequireDiagnostic("class B { void f() {} } class D : B { void f() {} }", "hides inherited method 'B.f', which is not virtual");
    RequireDiagnostic("class B { virtual void f() {} } class D : B { void f() {} }", "must be marked 'override'");
    RequireDiagnostic("class B { void f() {} } class D : B { override void f() {} }", "which is not virtual, abstract or override");
    RequireDiagnostic("class B {} class D : B { override void f() {} }", "no inherited virtual method has the same parameters");
    RequireDiagnostic("class B { virtual void f() {} } class D : B { sealed override void f() {} } class E : D { override void f() {} }", "cannot override sealed method 'D.f'");
    RequireDiagnostic("sealed class B {} class D : B {}", "cannot derive from sealed class 'B'");
    RequireDiagnostic("struct S {} class D : S {}", "cannot derive from struct 'S'");
    RequireDiagnostic("abstract class B { abstract void f(); } B b = new B();", "Cannot create an instance of abstract class 'B'");
    RequireDiagnostic("abstract class B { abstract void f(); } class D : B {}", "must be 'abstract' or override abstract method 'B.f'");
    RequireDiagnostic("class B { abstract void f(); }", "must be 'abstract' to declare abstract method");
    RequireDiagnostic("abstract class B { abstract void f() {} }", "cannot have a body");
    RequireDiagnostic("class B { void f(); }", "must have a body");
    RequireDiagnostic("class B { static virtual void f() {} }", "Static method 'f' cannot be virtual");
    RequireDiagnostic("struct S { virtual void f() {} }", "Struct method 'f' cannot be virtual");
    RequireDiagnostic("abstract struct S {}", "Structs cannot be 'abstract' or 'sealed'");
    RequireDiagnostic("class B { sealed void f() {} }", "Only an 'override' method can be 'sealed'");
    RequireDiagnostic("class B { virtual override void f() {} }", "can only be one of");
    RequireDiagnostic("class B { private: virtual void f() {} }", "cannot be private");
    RequireDiagnostic("class B { B(int x) {} } class D : B { D() {} }", "Base class 'B' has no parameterless constructor");
    RequireDiagnostic("class B { B(int x) {} } class D : B {}", "Base class 'B' has no parameterless constructor");
    RequireDiagnostic("class B { B(int x) {} } class D : B { D() : base(\"no\") {} }", "Constructor argument 1 for 'B' expects 'int'");
    RequireDiagnostic("class D { D() : base() {} }", "requires a base class");
    RequireDiagnostic("class B { B(int x) {} } class D : B { int y; D() : base(y) {} }", "Instance member 'y' cannot be used in base constructor arguments");
    RequireDiagnostic("class B { int x; } class D : B { int x; }", "conflicts with the field inherited from 'B'");
    RequireDiagnostic("class B { void f() {} } class D : B { void g() { auto b = base; } }", "'base' can only be used to access a member");
    RequireDiagnostic("abstract class B { abstract void f(); } class D : B { override void f() { base.f(); } }", "Cannot call abstract method 'B.f' through 'base'");
    RequireDiagnostic("class A {} class B {} A a = new A(); bool x = a is B;", "the classes are not related");
    RequireDiagnostic("int i = 1; bool x = i is int;", "requires a class or 'any' operand");
    RequireDiagnostic("table t = {}; auto x = t.v as int;", "'as' requires a reference type");
}

void TestStaticMethodsAndOverloads() {
    RequireClean(R"(class Util {
    static int twice(int x) { return x * 2; }
    static float twice(float x) { return x * 2; }
    static string twice(string s) { return s + s; }
    int base = 1;
    int add(int a) { return base + a; }
    int add(int a, int b) { return base + a + b; }
    int add(Vec3 v) { return base; }
    Util() {}
    Util(int b) { base = b; }
    static int viaStatic() { return twice(2); }
}
int i = Util.twice(2);
float f = Util.twice(1.5);
string s = Util.twice("ab");
Util u = new Util(4);
int r = u.add(1) + u.add(1, 2) + u.add(Vec3(1, 2, 3));
float m1 = math.min(1.5, 2);
int m2 = math.min(1, 2);
Vec3 m3 = math.min(Vec3(1, 2, 3), Vec3(3, 2, 1));
Vec2 m4 = math.max(Vec2(1, 2), Vec2(3, 0));
Vec3i m5 = math.clamp(Vec3i(1, 2, 3), Vec3i(0, 0, 0), Vec3i(2, 2, 2));
float len = math.length(Vec3(1, 2, 2));
float pi = math.pi;)", "valid overloaded program");
    const auto analysis = luasharp::Analyze("float a = math.min(1, 2);\nint b = math.min(1.5, 2);");
    Require(HasDiagnostic(analysis, "Cannot assign"), "overload result types were not distinguished");
    RequireDiagnostic("int bad = math.min(\"a\", 1);", "No overload of 'min'");
    RequireDiagnostic("class A { void f(int x) {} void f(int y) {} }", "already declared");
    RequireDiagnostic("class A { int f; void f(int x) {} }", "already declared");
    RequireDiagnostic("class A { A(int x) {} A(int y) {} }", "Constructor with the same parameters");
    RequireDiagnostic("class A { void f() {} } A.f();", "Instance method 'f' cannot be called on type 'A'");
    RequireDiagnostic("class A { static void f() {} } A a = new A(); a.f();", "Static method 'f' must be called on its type");
    RequireDiagnostic("class A { int x; static int f() { return x; } }", "Instance member 'x' cannot be used in a static method");
    RequireDiagnostic("class A { static int f() { return this; } }", "'this' cannot be used in a static method");
    RequireClean("class A { void f(int a) {} void f(float a) {} } A a = new A(); table t = {}; a.f(t.x);", "an `any` argument must not make overloads ambiguous");
    {
        // Equal-cost overloads are ambiguous unless an `any` argument decides at run time.
        const auto ambiguous = luasharp::Analyze("class X {} class Y {} class A { void f(X a, Y b) {} void f(Y a, X b) {} } A a = new A(); a.f(null, null);");
        Require(HasDiagnostic(ambiguous, "ambiguous"), "equally good overloads were not reported as ambiguous");
    }
}

struct NativeVec3 { float x, y, z; };
struct NativeQuat { float x, y, z, w; };
bool Near(float left, float right) { return left - right < 1e-4f && right - left < 1e-4f; }

void TestNativeStructs() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto module = luasharp::NativeModule::Compile(R"(struct Particle {
    Vec3 pos;
    float mass = 2;
    Particle(Vec3 p) { pos = p; }
    Particle(Vec3 p, float m) { pos = p; mass = m; }
    void push(Vec3 d) { pos = pos + d; }
    float weight() { return mass * 10; }
    static Particle origin() { return Particle(Vec3(0, 0, 0)); }
}
Particle p = Particle(Vec3(1, 2, 3));
p.push(Vec3(1, 1, 1));
Particle copy = p;
copy.pos.x = 100;
Particle heavy = new Particle(Vec3(0, 0, 0), 5);
float heavyWeight = heavy.weight();
Particle origin = Particle.origin();
Particle fresh = Particle(Vec3(1, 1, 1));
Particle[] many = {p, copy};
many[0].mass = 9;
float listMass = many[0].mass + p.mass;
Particle fromList = many[1];
table bag = {value = p.pos};
Vec3 unboxed = bag.value;
float viaTable = bag.value.y;
bool equal = unboxed == p.pos;
Vec3 sum = Vec3(1, 2, 3) + Vec3(1, 1, 1) * 2;
float dot = Vec3(1, 2, 3).dot(Vec3(4, 5, 6));
Quat turn = Quat.from_axis_angle(Vec3(0, 0, 1), math.pi / 2);
Vec3 turned = turn * Vec3(1, 0, 0);
Mat4x4 move = Mat4x4.from_pos(Vec3(1, 2, 3));
Vec3 moved = move * Vec3(1, 1, 1);
Vec3 back = move.inverse() * moved;
Vec2i grid = Vec2i(3, 4) * 2;
int gridLength = grid.length_squared();
float clamped = math.clamp(15.5, 0, 10);
int smaller = math.min(7, 3);
)");
    const auto vector = [&](std::string_view name) { return NativeGlobal<NativeVec3>(module, name); };
    const auto particlePos = vector("p");   // first field of Particle is its Vec3
    Require(Near(particlePos.x, 2) && Near(particlePos.y, 3) && Near(particlePos.z, 4), "struct method did not update the receiver in place");
    Require(Near(vector("copy").x, 100) && Near(vector("p").x, 2), "struct assignment shared storage instead of copying");
    Require(NativeGlobal<double>(module, "heavyWeight") == 50.0, "constructor argument was not stored");
    Require(NativeGlobal<double>(module, "listMass") == 11.0, "struct stored in a list aliased the original");
    Require(Near(vector("fromList").x, 100), "list element copy was wrong");
    Require(Near(NativeGlobal<NativeVec3>(module, "unboxed").y, 3) && NativeGlobal<double>(module, "viaTable") == 3.0, "struct did not survive boxing through a table");
    Require(NativeGlobal<unsigned char>(module, "equal") == 1, "struct equality was wrong");
    const auto sum = vector("sum");
    Require(Near(sum.x, 3) && Near(sum.y, 4) && Near(sum.z, 5), "native vector operators were wrong");
    Require(NativeGlobal<double>(module, "dot") == 32.0, "native vector method was wrong");
    const auto turned = vector("turned");
    Require(Near(turned.x, 0) && Near(turned.y, 1) && Near(turned.z, 0), "quaternion rotation was wrong");
    const auto moved = vector("moved");
    Require(Near(moved.x, 2) && Near(moved.y, 3) && Near(moved.z, 4), "Mat4x4 point transform was wrong");
    const auto back = vector("back");
    Require(Near(back.x, 1) && Near(back.y, 1) && Near(back.z, 1), "Mat4x4 inverse was wrong");
    Require(NativeGlobal<long long>(module, "gridLength") == 100, "integer vector operations were wrong");
    Require(NativeGlobal<double>(module, "clamped") == 10.0 && NativeGlobal<long long>(module, "smaller") == 3, "math overloads picked the wrong implementation");
}

void TestNativeStructBoundaries() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto module = luasharp::NativeModule::Compile(R"(struct Counter {
    int n;
    Counter(int start) { n = start; if (start < 0) { return; } n += 1; }
    void bump() { n += 1; }
}
class Holder {
    int run() {
        Counter local = Counter(0);
        auto f = () => { local.bump(); return local.n; };
        f(); f();
        return local.n;
    }
}
Counter c = Counter(-1);
Counter d = Counter(1);
Counter captured = Counter(10);
auto bumpIt = () => { captured.bump(); return captured.n; };
int first = bumpIt();
int second = bumpIt();
int outside = captured.n;
Holder holder = new Holder();
int viaLocal = holder.run();
string text = string(Vec3(1, 2, 3));
string quat = string(Quat.identity());
)");
    Require(NativeGlobal<long long>(module, "c") == -1 && NativeGlobal<long long>(module, "d") == 2, "bare return in a struct constructor was wrong");
    Require(NativeGlobal<long long>(module, "first") == 12 && NativeGlobal<long long>(module, "second") == 13 && NativeGlobal<long long>(module, "outside") == 13, "captured global struct was copied instead of shared");
    Require(NativeGlobal<long long>(module, "viaLocal") == 3, "captured local struct was copied instead of shared");
    const auto text = NativeGlobal<const NativeString*>(module, "text");
    Require(text && std::string_view(text->data, static_cast<size_t>(text->length)) == "Vec3(1.0, 2.0, 3.0)", "struct to string was wrong");
}

struct NativeClosure {
    void (*invoke)(void* environment, const luasharp::NativeValue* arguments, luasharp::NativeValue* result);
    void* environment;
    unsigned long long arity;
};
// Calls a zero-argument closure stored in a module global.
luasharp::NativeValue CallGlobalClosure(const luasharp::NativeModule& module, std::string_view name) {
    const auto* closure = NativeGlobal<const NativeClosure*>(module, name);
    Require(closure && closure->invoke, "missing native closure " + std::string(name));
    luasharp::NativeValue result;
    closure->invoke(closure->environment, nullptr, &result);
    return result;
}

void TestNativeStaticFields() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(class Counter {
    static int count = 10;
    static string label = "start";
    static int[] history;
    int id;
    Counter() { count += 1; id = count; history.push_back(id); }
    static int next() { return count + 1; }
    static void rename(string text) { label = text + "!"; }
}
struct Point {
    int x;
    void shift() { x += 1; }
    static Point origin = Point();
    static int made = Counter.count;
}
Counter first = new Counter();
Counter second = new Counter();
Counter.count += 100;
int afterBump = Counter.count;
int predicted = Counter.next();
int historySize = Counter.history.size();
Counter.rename("renamed");
Point.origin.shift();
Point.origin.shift();
int originX = Point.origin.x;
int madeAtInit = Point.made;
auto readLater = () => Counter.count;
)", options);
        Require(NativeGlobal<long long>(module, "afterBump") == 112 && NativeGlobal<long long>(module, "predicted") == 113, "static field writes or static method reads were wrong");
        Require(NativeGlobal<long long>(module, "historySize") == 2 && NativeGlobal<long long>(module, "second") != 0, "constructor did not update a static list");
        Require(NativeGlobal<long long>(module, "originX") == 2, "struct method on a static struct field did not change it in place");
        Require(NativeGlobal<long long>(module, "madeAtInit") == 10, "static initializers did not run first, in declaration order");
        Require(NativeGlobal<long long>(module, "Counter.count") == 112, "static field is not exported as a global");
        runtime->Collect();
        Require(NativeText(module, "Counter.label") == "renamed!", "collection freed a string held by a static field");
        const auto* history = NativeGlobal<const luasharp::NativeList*>(module, "Counter.history");
        Require(history && history->count == 2 && reinterpret_cast<const long long*>(history->data)[1] == 12, "collection freed a list held by a static field");
        const auto later = CallGlobalClosure(module, "readLater");
        Require(later.kind == luasharp::NativeValueKind::Int && later.as.integer == 112, "lambda did not read the static field");
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "static field graph was not reclaimed after module unload");
}

void TestNativeStructReferences() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(class Node { int value; Node(int v) { value = v; } }
struct Entry {
    string name;
    Node node;
    int[] values;
    table bag;
    Entry(string n, int v) { name = n; node = new Node(v); values = {v, v + 1}; bag = {key = "k" + n}; }
    int total() { return node.value + values[1]; }
    static Entry spare = Entry("spare", 30);
}
struct Outer { Entry inner; int tag; }
class Holder { Entry entry = Entry("held", 3); }
Entry global = Entry("global", 1);
Entry[] list = {Entry("a", 10), Entry("b", 20)};
Holder holder = new Holder();
Outer outer = Outer();
outer.inner = Entry("outer", 40);
table boxes = {item = Entry("boxed", 5)};
auto capture = () => { Entry local = Entry("cell", 7); auto get = () => local.total(); return get; };
auto getter = capture();
auto check = () => {
    Entry boxed = boxes.item;
    int sum = global.total() + list[0].total() + list[1].total() + holder.entry.total() + boxed.total() + int(getter()) + Entry.spare.total() + outer.inner.total();
    if (global.name != "global" || list[1].name != "b" || boxed.name != "boxed" || string(boxed.bag.key) != "kboxed" || string(holder.entry.bag.key) != "kheld" || outer.inner.name != "outer") { return -1; }
    return sum;
};
string text = string(boxes.item);
)", options);
        constexpr long long expected = 3 + 21 + 41 + 7 + 11 + 15 + 61 + 81;
        const auto before = CallGlobalClosure(module, "check");
        Require(before.kind == luasharp::NativeValueKind::Int && before.as.integer == expected, "struct reference fields were wrong before collection");
        for (int pass{}; pass < 3; ++pass) {
            runtime->Collect();
            const auto after = CallGlobalClosure(module, "check");
            Require(after.kind == luasharp::NativeValueKind::Int && after.as.integer == expected && module.LastError().empty(), "collection freed a reference held inside a struct");
        }
        Require(NativeText(module, "text").starts_with("Entry(boxed, table: "), "struct with references did not format: " + std::string(NativeText(module, "text")));
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "struct reference graph was not reclaimed after module unload");
}

void TestNativeStructTypeChecks() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    RequireNativeError("struct A { int x; } struct B { float y; } A[] items = {A()}; table t = {v = items}; B[] other = t.v;", "list element type mismatch: expected B, got A");
    RequireNativeError("class C {} class D {} C[] items = {new C()}; table t = {v = items}; D[] other = t.v;", "list element type mismatch: expected D, got C");
    RequireNativeError("int[][] items = {{1}}; table t = {v = items}; float[][] other = t.v;", "list element type mismatch: expected float[], got int[]");
    RequireNativeError("struct P { int x; } table t = {p = P()}; t.p.x = 3;", "cannot assign a field of a boxed P");
    auto module = luasharp::NativeModule::Compile(R"(table zeros = {zero = Vec3(0, 0, 0), negative = Vec3(0, 0, 0) * -1.0};
bool boxedEqual = zeros.zero == zeros.negative;
bool typedEqual = Vec3(0, 0, 0) == Vec3(0, 0, 0) * -1.0;
struct Key { int a; float b; string s; }
Key k = Key(); k.a = 1; k.b = 2; k.s = "x";
Key same = Key(); same.a = 1; same.b = 2; same.s = "x";
table byKey = {};
byKey[k] = 7;
int found = int(byKey[same]);
table pair = {left = k, right = same};
bool keysEqual = pair.left == pair.right;
struct P { float x; }
P[] ps = {P(), P()};
ps[0].x = 1; ps[1].x = 2;
table holder = {items = ps};
float sum = 0;
for (p in holder.items) { sum += float(p.x); }
)");
    Require(NativeGlobal<unsigned char>(module, "boxedEqual") == 1 && NativeGlobal<unsigned char>(module, "typedEqual") == 1, "boxed and typed struct equality disagree on -0.0");
    Require(NativeGlobal<long long>(module, "found") == 7 && NativeGlobal<unsigned char>(module, "keysEqual") == 1, "equal structs were different table keys");
    Require(NativeGlobal<double>(module, "sum") == 3.0, "foreach over a struct list held in `any` was wrong");
}

// A struct boxed by one module unboxes in another only when both declare the same layout.
void TestNativeStructAcrossModules() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    luasharp::NativeCompileOptions options;
    options.runtime = runtime;
    auto source = luasharp::NativeModule::Compile("struct P { int a; P(int v) { a = v; } }\ntable box = {value = P(4)};\nauto boxed = box.value;", options);
    const auto boxed = NativeGlobal<luasharp::NativeValue>(source, "boxed");
    Require(boxed.kind == luasharp::NativeValueKind::Struct, "struct was not boxed");
    const auto read = [&](std::string_view declaration) {
        auto reader = luasharp::NativeModule::Compile(std::string(declaration) + "\ntable none = {};\nauto input = none.missing;\nauto read = () => { P p = input; return p.a; };", options);
        *static_cast<luasharp::NativeValue*>(reader.FindGlobal("input")) = boxed;
        const auto result = CallGlobalClosure(reader, "read");
        return std::pair{result.kind == luasharp::NativeValueKind::Int ? result.as.integer : -1, reader.LastError()};
    };
    const auto [same, sameError] = read("struct P { int a; P(int v) { a = v; } }");
    Require(same == 4 && sameError.empty(), "a struct with the same layout did not cross modules: " + sameError);
    const auto [wider, widerError] = read("struct P { int a; int b; }");
    Require(wider == 0 && widerError == "expected P, got P", "a struct with a different layout crossed modules: " + widerError);
}

void TestNativeInheritance() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto runtime = luasharp::NativeRuntime::Create();
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime;
        auto module = luasharp::NativeModule::Compile(R"(abstract class Shape {
    string name;
    int built;
    static string log = "";
    Shape(string n) { name = n; log = log + "S"; built = kind(); }
    abstract int area();
    virtual int kind() { return 1; }
    virtual string describe() { return name + ":" + string(area()); }
    int sides() { return 0; }
}
class Node { int v; Node(int x) { v = x; } }
class Rect : Shape {
    int w = 1;
    int h = 1;
    Node extra = new Node(5);
    Rect(int w, int h) : base("rect") { this.w = w; this.h = h; log = log + "R"; }
    Rect(int s) : base("sq" + string(s)) { w = s; h = s; }
    override int area() { return w * h; }
    override int kind() { return 2; }
    override string describe() { return "[" + base.describe() + "]"; }
}
sealed class Cube : Rect {
    int d = 3;
    Cube() : base(2, 2) { log = log + "C"; }
    override int area() { return base.area() * d; }
}
Shape a = new Rect(2, 3);
Shape b = new Cube();
Rect c = new Rect(4);
int areaA = a.area();
int areaB = b.area();
string descA = a.describe();
string descB = b.describe();
string descC = c.describe();
int builtB = b.built;
int sides = b.sides();
string order = Shape.log;
bool aIsRect = a is Rect;
bool aIsCube = a is Cube;
bool bIsCube = b is Cube;
Shape nothing = null;
bool nullIs = nothing is Rect;
Cube asCube = b as Cube;
Cube asNone = a as Cube;
int cubeD = asCube.d;
bool asNull = asNone == null;
table t = {s = b};
bool anyIsRect = t.s is Rect;
bool anyIsString = t.s is string;
Rect fromAny = t.s;
int fromAnyArea = fromAny.area();
bool anyAsOk = (t.s as Rect) != null;
int dynArea = int(t.s.area());
string dynDescribe = string(t.s.describe());
Shape[] shapes = {a, b, c};
int total = 0;
for (s in shapes) { total += s.area(); }
auto check = () => asCube.describe() + string(asCube.extra.v) + asCube.name;
)", options);
        Require(NativeGlobal<long long>(module, "areaA") == 6 && NativeGlobal<long long>(module, "areaB") == 12, "virtual calls did not dispatch to the override");
        Require(NativeText(module, "descA") == "[rect:6]" && NativeText(module, "descB") == "[rect:12]" && NativeText(module, "descC") == "[sq4:16]", "base calls or inherited virtual calls were wrong: " + std::string(NativeText(module, "descB")));
        Require(NativeGlobal<long long>(module, "builtB") == 2, "a virtual call in a base constructor did not dispatch to the derived class");
        Require(NativeGlobal<long long>(module, "sides") == 0, "inherited non-virtual method call was wrong");
        Require(NativeText(module, "order") == "SRSRCS", "constructors ran in the wrong order: " + std::string(NativeText(module, "order")));
        Require(NativeGlobal<unsigned char>(module, "aIsRect") == 1 && NativeGlobal<unsigned char>(module, "aIsCube") == 0 && NativeGlobal<unsigned char>(module, "bIsCube") == 1 && NativeGlobal<unsigned char>(module, "nullIs") == 0, "'is' on class values was wrong");
        Require(NativeGlobal<long long>(module, "cubeD") == 3 && NativeGlobal<unsigned char>(module, "asNull") == 1, "'as' on class values was wrong");
        Require(NativeGlobal<unsigned char>(module, "anyIsRect") == 1 && NativeGlobal<unsigned char>(module, "anyIsString") == 0 && NativeGlobal<unsigned char>(module, "anyAsOk") == 1, "'is' or 'as' on `any` was wrong");
        Require(NativeGlobal<long long>(module, "fromAnyArea") == 12, "a derived object did not unbox from `any` to its base class");
        Require(NativeGlobal<long long>(module, "dynArea") == 12 && NativeText(module, "dynDescribe") == "[rect:12]", "dynamic calls did not use the most-derived methods");
        Require(NativeGlobal<long long>(module, "total") == 34, "virtual calls in a loop over base-typed elements were wrong");
        Require(module.LastError().empty(), "inheritance program reported a runtime error: " + module.LastError());
        for (int pass{}; pass < 3; ++pass) {
            runtime->Collect();
            const auto checked = CallGlobalClosure(module, "check");
            Require(checked.kind == luasharp::NativeValueKind::String, "check closure did not return a string");
            const auto* text = static_cast<const NativeString*>(checked.as.pointer);
            Require(std::string_view(text->data, static_cast<size_t>(text->length)) == "[rect:12]5rect", "collection freed a field declared in a base class");
        }
    }
    Require(runtime->Collect() > 0 && runtime->AllocationCount() == 0, "inheritance graph was not reclaimed after module unload");
    RequireNativeError("class B {} class D : B {} class E {} table t = {v = new D()}; E e = t.v;", "expected E, got D");
}

// A host "engine" for the extern class tests. Thunks are plain functions, so the state is global.
struct HostEntity { long long health{}; long long level{}; };
struct HostWorld {
    std::shared_ptr<luasharp::NativeRuntime> runtime;
    std::vector<std::unique_ptr<HostEntity>> scriptOwned; // deleted by the proxy finalizer
    std::vector<std::unique_ptr<HostEntity>> engineOwned;
    std::vector<void*> engineProxies;
    long long created{};
};
HostWorld g_world;

void FinalizeScriptOwned(void* proxy) {
    auto* native = luasharp::NativeRuntime::NativeOf(proxy);
    std::erase_if(g_world.scriptOwned, [&](const auto& entity) { return entity.get() == native; });
}
void* EntityNew(long long health) {
    auto& entity = g_world.scriptOwned.emplace_back(std::make_unique<HostEntity>(HostEntity{health}));
    ++g_world.created;
    return g_world.runtime->NewNativeProxy(g_world.runtime->FindNativeClass("Entity"), entity.get(), FinalizeScriptOwned);
}
void* EngineEntity(std::string_view type, long long health, long long level) {
    auto& entity = g_world.engineOwned.emplace_back(std::make_unique<HostEntity>(HostEntity{health, level}));
    return g_world.engineProxies.emplace_back(g_world.runtime->NewNativeProxy(g_world.runtime->FindNativeClass(type), entity.get()));
}
long long EntityGetHealth(void* self) { return static_cast<HostEntity*>(self)->health; }
void EntitySetHealth(void* self, long long value) { static_cast<HostEntity*>(self)->health = value; }
long long EntityDamage(void* self, long long amount) { static_cast<HostEntity*>(self)->health -= amount; return amount; }
long long EntityCount() { return g_world.created; }
void* EntitySpawn(void*, long long health) { return EngineEntity("Entity", health, 0); }
unsigned char EntitySame(void* self, void* other) { return self == other; }
long long PlayerLevel(void* self) { return static_cast<HostEntity*>(self)->level; }
void* PlayerFind(long long level) { return EngineEntity("Player", 100, level); }

constexpr std::string_view EntityDeclarations = R"(extern class Entity {
    int health;
    Entity(int health);
    virtual int damage(int amount);
    static int count();
    virtual Entity spawn(int health);
    virtual bool same(Entity other);
}
extern class Player : Entity { virtual int level(); static Player find(int level); })";

std::vector<luasharp::NativeSymbol> EntitySymbols() {
    return {{"ls_native_Entity_new", reinterpret_cast<const void*>(&EntityNew)}, {"ls_native_Entity__get_health", reinterpret_cast<const void*>(&EntityGetHealth)},
            {"ls_native_Entity__set_health", reinterpret_cast<const void*>(&EntitySetHealth)}, {"ls_native_Entity_damage", reinterpret_cast<const void*>(&EntityDamage)},
            {"ls_native_Entity_count", reinterpret_cast<const void*>(&EntityCount)}, {"ls_native_Entity_spawn", reinterpret_cast<const void*>(&EntitySpawn)},
            {"ls_native_Entity_same", reinterpret_cast<const void*>(&EntitySame)}, {"ls_native_Player_level", reinterpret_cast<const void*>(&PlayerLevel)},
            {"ls_native_Player_find", reinterpret_cast<const void*>(&PlayerFind)}};
}

void TestExternClassSemantics() {
    const auto has = [](const luasharp::Analysis& analysis, std::string_view message) {
        return std::any_of(analysis.diagnostics.begin(), analysis.diagnostics.end(), [&](const auto& item) { return item.message.find(message) != std::string::npos; });
    };
    Require(has(luasharp::Analyze("extern class A { int f() { return 1; } }"), "cannot have a body"), "extern method body was not diagnosed");
    Require(has(luasharp::Analyze("extern class A { int x = 1; }"), "cannot have an initializer"), "extern field initializer was not diagnosed");
    Require(has(luasharp::Analyze("extern class A { static int x; }"), "cannot declare static field"), "extern static field was not diagnosed");
    Require(has(luasharp::Analyze("extern class A { A(int x); A(float x); }"), "only one constructor"), "extern constructor overload was not diagnosed");
    Require(has(luasharp::Analyze("extern class A { virtual void f(int x); virtual void f(float x); }"), "cannot be overloaded"), "extern method overload was not diagnosed");
    Require(has(luasharp::Analyze("extern class A { abstract void f(); }"), "can only be 'static' or 'virtual'"), "extern method modifier was not diagnosed");
    Require(has(luasharp::Analyze("class B {} extern class A : B {}"), "can only derive from an extern class"), "extern class with a script base was not diagnosed");
    Require(has(luasharp::Analyze("extern class A {} A a = new A();"), "declares no constructor"), "extern class without a constructor was constructed");
    Require(has(luasharp::Analyze("extern struct A {}"), "Only a class can be 'extern'"), "extern struct was not diagnosed");
    Require(luasharp::Analyze("extern class A { int x; A(int x); virtual int f(); static A g(); } A a = new A(1); int y = a.f() + a.x; a.x = 2;").diagnostics.empty(), "valid extern class use produced diagnostics");
    const auto generated = luasharp::CompileToC(std::string(EntityDeclarations) + "\nEntity e = new Entity(1);\nint h = e.health;\ne.health = 2;\nbool alive = e != null;");
    for (const auto expected : {"typedef LS_NativeObject LS_Entity;", "LS_KIND_OBJECT, &ls_type_Entity, (void* const*)0, LS_TYPE_NATIVE};","extern ls_int ls_native_Entity__get_health(void* self);",
                                "extern ls_bool ls_native_Entity_same(void* self, void* other);", "ls_native_ptr(other)", "ls_native_self(self, \"Entity.damage\")", "ls_Entity__set_health(",
                                "extern LS_Player* ls_native_Player_find(ls_int level);"})
        Require(generated.c.find(expected) != std::string::npos, std::string("missing extern class lowering: ") + expected);
    Require(generated.nativeClasses.size() == 2 && generated.nativeClasses[0].name == "Entity" && generated.nativeClasses[1].baseName == "Entity", "extern classes were not reported base first");
    bool rejected = false;
    try { static_cast<void>(luasharp::DeclarationTypes({"class A {}"})); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "a declaration source with script code was accepted");
}

void TestNativeExternClasses() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    g_world = {}; g_world.runtime = luasharp::NativeRuntime::Create();
    auto& runtime = g_world.runtime;
    {
        luasharp::NativeCompileOptions options;
        options.runtime = runtime; options.declarations = {std::string(EntityDeclarations)}; options.symbols = EntitySymbols();
        auto module = luasharp::NativeModule::Compile(R"(Entity e = new Entity(10);
int hp0 = e.health;
e.health = 25;
e.health += 5;
int hp1 = e.health;
int dealt = e.damage(7);
int hp2 = e.health;
int created = Entity.count();
Entity child = e.spawn(3);
int childHp = child.health;
bool sameSelf = e.same(e);
bool sameOther = e.same(child);
bool sameNull = e.same(null);
Player p = Player.find(4);
int level = p.level();
int pDealt = p.damage(1);
Entity asEntity = p;
bool pIsPlayer = asEntity is Player;
bool eIsPlayer = e is Player;
Player back = asEntity as Player;
bool backOk = back != null;
Entity again = back;
bool eqSame = asEntity == again;
table t = {v = p};
bool anyIsPlayer = t.v is Player;
Player fromAny = t.v;
int anyLevel = fromAny.level();
auto childNull = () => child == null;
table held = {v = child};
auto childFalsy = () => !held.v;
auto childIsEntity = () => child is Entity;
auto hitChild = () => child.damage(1);
auto childHealth = () => child.health;
)", options);
        Require(NativeGlobal<long long>(module, "hp0") == 10 && NativeGlobal<long long>(module, "hp1") == 30 && NativeGlobal<long long>(module, "hp2") == 23, "extern field accessors were wrong");
        Require(NativeGlobal<long long>(module, "dealt") == 7 && NativeGlobal<long long>(module, "created") == 1 && NativeGlobal<long long>(module, "childHp") == 3, "extern method calls were wrong");
        Require(NativeGlobal<unsigned char>(module, "sameSelf") == 1 && NativeGlobal<unsigned char>(module, "sameOther") == 0 && NativeGlobal<unsigned char>(module, "sameNull") == 0, "extern arguments did not pass host pointers");
        Require(NativeGlobal<long long>(module, "level") == 4 && NativeGlobal<long long>(module, "pDealt") == 1, "derived extern class calls were wrong");
        Require(NativeGlobal<unsigned char>(module, "pIsPlayer") == 1 && NativeGlobal<unsigned char>(module, "eIsPlayer") == 0 && NativeGlobal<unsigned char>(module, "backOk") == 1 && NativeGlobal<unsigned char>(module, "eqSame") == 1, "'is', 'as' or '==' on extern classes was wrong");
        Require(NativeGlobal<unsigned char>(module, "anyIsPlayer") == 1 && NativeGlobal<long long>(module, "anyLevel") == 4, "extern objects did not round-trip through `any`");
        Require(module.LastError().empty(), "extern class program reported a runtime error: " + module.LastError());
        Require(CallGlobalClosure(module, "childNull").as.boolean == 0, "a live proxy compared equal to null");

        luasharp::NativeRuntime::DestroyNative(g_world.engineProxies.front());
        Require(CallGlobalClosure(module, "childNull").as.boolean == 1, "a destroyed proxy did not compare equal to null");
        Require(CallGlobalClosure(module, "childFalsy").as.boolean == 1 && CallGlobalClosure(module, "childIsEntity").as.boolean == 0, "a destroyed proxy was truthy or matched 'is'");
        Require(module.LastError().empty(), "null checks on a destroyed proxy reported an error");
        Require(CallGlobalClosure(module, "hitChild").as.integer == 0 && CallGlobalClosure(module, "childHealth").as.integer == 0, "a call on a destroyed proxy did not return the default");
        Require(module.LastError().find("destroyed or null") != std::string::npos, "a call on a destroyed proxy did not report an error: " + module.LastError());
        module.ClearError();

        // A second module emits its own Entity type info; it matches the runtime's by name.
        luasharp::NativeCompileOptions second = options; second.declarations.clear();
        auto other = luasharp::NativeModule::Compile(std::string(EntityDeclarations) + "\nEntity made = new Entity(5);\ntable t = {v = made, p = Player.find(2)};\nbool isEntity = t.v is Entity;\nbool isPlayer = t.p is Player;\nPlayer p = t.p;\nEntity[] list = {made, p};", second);
        Require(NativeGlobal<unsigned char>(other, "isEntity") == 1 && NativeGlobal<unsigned char>(other, "isPlayer") == 1 && other.LastError().empty() && g_world.scriptOwned.size() == 2,
                "an inline extern class did not match the runtime type info");
        bool conflict = false;
        try { static_cast<void>(luasharp::NativeModule::Compile("extern class Player { virtual int level(); }", second)); }
        catch (const std::runtime_error& error) { conflict = std::string_view(error.what()).find("different base") != std::string_view::npos; }
        Require(conflict, "an extern class with a different base was accepted");
        runtime->Collect();
        Require(g_world.scriptOwned.size() == 2, "a collection freed reachable script-owned objects");
    }
    runtime->Collect();
    Require(g_world.scriptOwned.empty(), "script-owned host objects were not finalized with their proxies");

    // NativeRef keeps a proxy alive until Reset.
    {
        luasharp::NativeRef held(runtime, EntityNew(1));
        runtime->Collect();
        Require(g_world.scriptOwned.size() == 1 && luasharp::NativeRuntime::NativeOf(held.Get()), "a NativeRef did not keep its proxy alive");
        luasharp::NativeRef moved = std::move(held);
        runtime->Collect();
        Require(g_world.scriptOwned.size() == 1 && !held.Get() && moved.Get(), "a moved NativeRef did not keep its proxy alive");
        moved.Reset();
        runtime->Collect();
        Require(g_world.scriptOwned.empty(), "a reset NativeRef still kept its proxy alive");
    }
    g_world = {};
}

void TestInheritanceEmission() {
    const auto generated = luasharp::CompileToC(R"(class B { int x; virtual int f() { return 1; } int g() { return f(); } }
sealed class D : B { int y; override int f() { return base.f() + 1; } }
B b = new D();
D d = new D();
int viaBase = b.f();
int viaSealed = d.f();
int inherited = d.g();)");
    for (const auto expected : {"struct LS_D {\n    const LS_TypeInfo* ls_type;\n    ls_int x;\n    ls_int y;\n};", "static void* const ls_vtable_D[] = {", "(void*)ls_D_f, /* f() */",
                                "&ls_type_B, ls_vtable_D};", "void ls_D__init(LS_D* self) {\n    ls_B__init((LS_B*)self);", "->ls_type->vtable[0])(", "ls_B_f(((LS_B*)self))",
                                "ls_state.viaSealed = ls_D_f(ls_state.d);", "ls_state.inherited = ls_B_g((LS_B*)ls_state.d);"})
        Require(generated.c.find(expected) != std::string::npos, std::string("missing inheritance lowering: ") + expected);
}

// Every native function the registry advertises must exist in the generated C header.
void TestNativeRegistryMatchesHeader() {
    if (!luasharp::NativeModule::IsLibTccAvailable()) return;
    auto generated = luasharp::CompileToC("int x = 1;");
    std::string table = "\nstatic const void* ls_registry_check[] = {\n";
    size_t count{};
    const auto reference = [&](const std::string& name) { if (!name.empty() && name.find('(') == std::string::npos) { table += "    (const void*)" + name + ",\n"; ++count; } };
    std::vector<std::string> owners = luasharp::NativeStructTypes();
    owners.push_back("math");
    for (const auto& owner : owners) {
        for (const bool isStatic : {false, true}) for (const auto* member : luasharp::BuiltinMembers(owner, isStatic)) if (member->kind == luasharp::SymbolKind::Method) reference(member->cName);
        for (const auto* member : luasharp::BuiltinMembers(owner, true)) if (member->kind == luasharp::SymbolKind::Field && member->cName.find("()") != std::string::npos) { table += "    (const void*)" + member->cName.substr(0, member->cName.size() - 2) + ",\n"; ++count; }
    }
    for (const auto& type : owners) if (type != "math") { table += "    (const void*)ls_" + type + "_eq,\n    (const void*)ls_" + type + "_default,\n"; count += 2; }
    table += "};\n";
    Require(count > 200, "registry check collected too few functions");
    generated.c += table;
    static_cast<void>(luasharp::NativeModule::Load(std::move(generated), {}));
}

void TestStructEmission() {
    const auto generated = luasharp::CompileToC("struct P { int a; Vec3 v; static int make(int x) { return x; } P(int a) { this.a = a; } }\nP p = P(1);\nint r = P.make(2);");
    for (const auto expected : {"struct LS_P {", "LS_P ls_P_new(ls_int a)", "LS_P ls_P_default(void)", "ls_bool ls_P_eq(LS_P a, LS_P b)", "ls_int ls_P_make(ls_int x)", "((*self)).a = a"})
        Require(generated.c.find(expected) != std::string::npos, std::string("missing struct lowering: ") + expected);
    Require(generated.c.find("ls_trace_P") == std::string::npos, "structs must not have a GC trace function");
    const auto overloads = luasharp::CompileToC("class A { int f(int x) { return x; } int f(float x) { return 1; } }");
    Require(overloads.c.find("ls_A_f(LS_A* self, ls_int x)") != std::string::npos && overloads.c.find("ls_A_f__1(LS_A* self, ls_float x)") != std::string::npos, "overloaded functions did not get distinct C names");
    Require(std::any_of(overloads.exports.begin(), overloads.exports.end(), [](const auto& item) { return item.sourceName == "A.f(float)" && item.symbolName == "ls_A_f__1"; }), "overload export name is wrong");
}
int main() {
    const std::vector<std::pair<std::string_view, std::function<void()>>> tests{
        {"invalid syntax", TestInvalidSyntax},
        {"semantic analysis", TestSemanticAnalysis},
        {"semantic diagnostics and parameters", TestSemanticDiagnosticsAndParameters},
        {"shared syntax tree", TestSharedSyntaxTree},
        {"expression tree", TestExpressionTree},
        {"recovering syntax tree", TestRecoveringSyntaxTree},
        {"lexer trivia and spans", TestLexerTriviaAndSpans},
        {"scope-aware binding", TestScopeAwareBinding},
        {"bound member chain", TestBoundMemberChain},
        {"semantic errors", TestSemanticErrors},
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
        {"built-in nullability", TestBuiltinNullability},
        {"loops and populated lists", TestLoopsAndPopulatedLists},
        {"compound assignments and flow warnings", TestCompoundAssignmentsAndFlowWarnings},
        {"keyed table literals", TestKeyedTableLiterals},
        {"for, break, and continue", TestForBreakAndContinue},
        {"foreach and range loops", TestForEachAndRangeLoops},
        {"enums", TestEnums},
        {"lambdas", TestLambdas},
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
        {"struct semantics", TestStructSemantics},
        {"static field semantics", TestStaticFieldSemantics},
        {"static methods and overloads", TestStaticMethodsAndOverloads},
        {"inheritance semantics", TestInheritanceSemantics},
        {"inheritance C emission", TestInheritanceEmission},
        {"struct C emission", TestStructEmission},
        {"native structs", TestNativeStructs},
        {"native struct boundaries", TestNativeStructBoundaries},
        {"native static fields", TestNativeStaticFields},
        {"native struct references", TestNativeStructReferences},
        {"native struct type checks", TestNativeStructTypeChecks},
        {"native structs across modules", TestNativeStructAcrossModules},
        {"native inheritance", TestNativeInheritance},
        {"extern class semantics", TestExternClassSemantics},
        {"native extern classes", TestNativeExternClasses},
        {"native registry matches header", TestNativeRegistryMatchesHeader},
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
