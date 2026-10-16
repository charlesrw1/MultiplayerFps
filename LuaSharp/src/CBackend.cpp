#include "CBackend.h"

#include "Builtins.h"
#include "Semantic.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace luasharp {
namespace {

std::string Identifier(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) result += std::isalnum(character) || character == '_' ? static_cast<char>(character) : '_';
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front()))) result.insert(result.begin(), '_');
    return result;
}

std::string CType(std::string_view type, const std::unordered_set<std::string>& enums) {
    if (type == "void") return "void";
    if (type == "int" || enums.contains(std::string(type))) return "ls_int";
    if (type == "float") return "ls_float";
    if (type == "bool") return "ls_bool";
    if (type == "string") return "ls_string";
    if (type == "table") return "ls_table";
    if (type == "any") return "LS_Value";
    if (type == "null") return "void*";
    if (type.starts_with("$function:")) return "LS_Closure*";
    if (type.ends_with("[]"))
        throw std::runtime_error("Native C backend does not support type '" + std::string(type) + "' yet");
    if (IsBuiltinType(type)) throw std::runtime_error("Native C backend requires a registered ABI for builtin type '" + std::string(type) + "'");
    return "LS_" + Identifier(type) + "*";
}

struct CEmitter {
    const Analysis& analysis;
    const BoundProgram& program;
    const TypeTable& types;
    const std::vector<ExternalType>& externalTypes;
    std::unordered_set<std::string> enums;
    std::unordered_set<std::string> externalNames;
    std::unordered_map<std::string, const BoundClass*> classes;
    std::unordered_set<size_t> globalSymbols;
    std::vector<const BoundExpression*> lambdas;
    std::vector<std::vector<const VariableBinding*>> lambdaCaptures;
    std::optional<size_t> activeLambda;
    std::string output;
    size_t nextLabel{};

    CEmitter(const Analysis& value, const std::vector<ExternalType>& external) : analysis(value), program(value.bound), types(value.types), externalTypes(external) {
        for (const auto& item : program.enums) enums.insert(item.name);
        for (const auto& item : program.classes) classes[item.name] = &item;
        for (const auto& item : externalTypes) { externalNames.insert(item.name); if (item.isEnum) enums.insert(item.name); }
        for (const auto& statement : program.Statements(program.topLevel)) {
            if (statement.kind != SyntaxStatement::Kind::Local) continue;
            for (const auto& binding : analysis.bindings) {
                if (binding.depth == 0 && binding.name == statement.Name() && binding.declaration.offset >= statement.span.offset && binding.declaration.offset <= statement.span.End()) {
                    globalSymbols.insert(binding.symbolIndex); break;
                }
            }
        }
        const auto expression = [&](auto&& self, const BoundExpression& value) -> void {
            if (value.kind == SyntaxExpression::Kind::Lambda) lambdas.push_back(&value);
            for (const auto& child : program.Children(value)) self(self, child);
            const auto statements = [&](auto&& statementSelf, NodeRange values) -> void {
                for (const auto& statement : program.Statements(values)) {
                    self(self, program.Expression(statement.expression)); self(self, program.Expression(statement.increment));
                    statementSelf(statementSelf, statement.initializer); statementSelf(statementSelf, statement.body); statementSelf(statementSelf, statement.elseBody);
                }
            };
            statements(statements, value.lambdaBody);
        };
        const auto statements = [&](auto&& self, NodeRange values) -> void {
            for (const auto& statement : program.Statements(values)) {
                expression(expression, program.Expression(statement.expression)); expression(expression, program.Expression(statement.increment));
                self(self, statement.initializer); self(self, statement.body); self(self, statement.elseBody);
            }
        };
        statements(statements, program.topLevel);
        for (const auto& type : program.classes) {
            for (const auto& field : type.fields) expression(expression, program.Expression(field.initializer));
            for (const auto& function : type.functions) statements(statements, function.body);
        }
        for (const auto* lambda : lambdas) {
            std::vector<const VariableBinding*> captured;
            const auto add = [&](const VariableBinding* binding) {
                if (!binding || globalSymbols.contains(binding->symbolIndex)) return;
                if (std::none_of(captured.begin(), captured.end(), [&](const auto* value) { return value->symbolIndex == binding->symbolIndex; })) captured.push_back(binding);
            };
            const auto scanExpression = [&](auto&& self, const BoundExpression& value) -> void {
                if (&value != lambda && value.kind == SyntaxExpression::Kind::Lambda) return;
                if (value.symbolIndex) {
                    const auto binding = std::find_if(analysis.bindings.begin(), analysis.bindings.end(), [&](const auto& item) { return item.symbolIndex == *value.symbolIndex; });
                    if (binding != analysis.bindings.end() && (binding->declaration.offset < lambda->span.offset || binding->declaration.offset >= lambda->span.End())) add(&*binding);
                }
                if (value.lowering == Lowering::ImplicitField || value.lowering == Lowering::ImplicitMethod) {
                    const auto binding = std::find_if(analysis.bindings.begin(), analysis.bindings.end(), [&](const auto& item) { return item.name == "this" && item.visibility.offset <= lambda->span.offset && item.visibility.End() >= lambda->span.End(); });
                    if (binding != analysis.bindings.end()) add(&*binding);
                }
                for (const auto& child : program.Children(value)) self(self, child);
                const auto scanStatements = [&](auto&& statementSelf, NodeRange values) -> void {
                    for (const auto& statement : program.Statements(values)) { self(self, program.Expression(statement.expression)); self(self, program.Expression(statement.increment)); statementSelf(statementSelf, statement.initializer); statementSelf(statementSelf, statement.body); statementSelf(statementSelf, statement.elseBody); }
                };
                scanStatements(scanStatements, value.lambdaBody);
            };
            scanExpression(scanExpression, *lambda);
            lambdaCaptures.push_back(std::move(captured));
        }
    }

    void Line(std::string_view indent, std::string_view text = {}) { output.append(indent); output.append(text); output += '\n'; }
    const std::string& Name(TypeId type) const { return types.Name(type); }
    std::string Type(std::string_view type) const { return CType(type, enums); }
    std::string Type(TypeId type) const { return CType(Name(type), enums); }
    bool IsEnum(TypeId type) const { return enums.contains(Name(type)); }
    bool IsManagedObject(std::string_view type) const { return type == "string" || type == "table" || type == "any" || type.starts_with("$function:") || classes.contains(std::string(type)); }
    static std::string Function(std::string_view owner, std::string_view name) { return "ls_" + Identifier(owner) + "_" + Identifier(name); }
    const BoundExpression& Child(const BoundExpression& value, size_t index) const { return program.Child(value, index); }
    const Symbol* ExternalMember(std::string_view owner, std::string_view name) const {
        const auto type = std::find_if(externalTypes.begin(), externalTypes.end(), [&](const auto& value) { return value.name == owner; });
        if (type == externalTypes.end()) return nullptr;
        const auto member = std::find_if(type->members.begin(), type->members.end(), [&](const auto& value) { return value.name == name; });
        return member == type->members.end() ? nullptr : &*member;
    }

    std::string Default(std::string_view type) const {
        if (type == "float") return "0.0";
        if (type == "string") return "ls_intern(\"\")";
        if (type == "table") return "((ls_table)0)";
        if (type == "any") return "((LS_Value){0})";
        if (type.starts_with("$function:")) return "((LS_Closure*)0)";
        if (type == "int" || type == "bool" || enums.contains(std::string(type))) return "0";
        if (!type.empty() && !IsBuiltinType(type)) return "((" + Type(type) + ")0)";
        throw std::runtime_error("Native C backend cannot default-initialize type '" + std::string(type) + "'");
    }

    std::string Box(const BoundExpression& value, std::string_view owner = {}) {
        const auto expression = Expression(value, owner);
        const auto type = value.type;
        if (type == TypeTable::Any) return expression;
        if (type == TypeTable::Null) return "((LS_Value){.kind=0})";
        if (type == TypeTable::Int || IsEnum(type)) return "((LS_Value){.kind=1,.as.integer=(" + expression + ")})";
        if (type == TypeTable::Float) return "((LS_Value){.kind=2,.as.number=(" + expression + ")})";
        if (type == TypeTable::Bool) return "((LS_Value){.kind=3,.as.boolean=(" + expression + ")})";
        if (type == TypeTable::String) return "((LS_Value){.kind=4,.as.pointer=(" + expression + ")})";
        if (type == TypeTable::Table) return "((LS_Value){.kind=6,.as.pointer=(" + expression + ")})";
        if (types.IsFunction(type)) return "((LS_Value){.kind=7,.as.pointer=(" + expression + ")})";
        if (classes.contains(Name(type))) return "((LS_Value){.kind=5,.as.pointer=(" + expression + ")})";
        throw std::runtime_error("Native C backend cannot box type '" + Name(type) + "'");
    }

    std::string Expression(const BoundExpression& value, std::string_view owner = {}) {
        const auto child = [&](size_t index) { return Expression(Child(value, index), owner); };
        const auto& text = value.Text();
        const size_t childCount = value.children.count;
        switch (value.kind) {
        case SyntaxExpression::Kind::Missing: return {};
        case SyntaxExpression::Kind::Identifier:
            if (text == "this") return "self";
            if (value.lowering == Lowering::ImplicitField) return "self->" + Identifier(text);
            if (value.symbolIndex && globalSymbols.contains(*value.symbolIndex)) return "ls_state." + Identifier(text);
            if (activeLambda && value.symbolIndex) for (const auto* capture : lambdaCaptures.at(*activeLambda)) if (capture->symbolIndex == *value.symbolIndex)
                return "ls_env->" + Identifier(capture->name);
            return Identifier(text);
        case SyntaxExpression::Kind::Number: return text;
        case SyntaxExpression::Kind::String: return "ls_intern(" + text + ")";
        case SyntaxExpression::Kind::Boolean: return text == "true" ? "1" : "0";
        case SyntaxExpression::Kind::Null: return "((void*)0)";
        case SyntaxExpression::Kind::Parenthesized: return "(" + child(0) + ")";
        case SyntaxExpression::Kind::New: {
            std::string result = Function(text, "new") + "(";
            for (size_t i{}; i < childCount; ++i) { if (i) result += ", "; result += child(i); }
            return result + ")";
        }
        case SyntaxExpression::Kind::Member: {
            const auto target = Child(value, 0).type;
            if (types.Kind(target) == TypeKind::Meta) return "LS_" + Identifier(types.Target(target)) + "_" + Identifier(text);
            if (externalNames.contains(Name(target))) if (const auto* symbol = ExternalMember(Name(target), text); symbol && symbol->kind == SymbolKind::Field)
                throw std::runtime_error("Native C backend cannot directly access field '" + text + "' on opaque external type '" + Name(target) + "'");
            return child(0) + "->" + Identifier(text);
        }
        case SyntaxExpression::Kind::Call: {
            const auto& callee = Child(value, 0);
            const auto argument = [&](size_t index) { return child(index + 1); };
            if (value.lowering == Lowering::Printf) {
                const size_t count = childCount - 2;
                if (!count) return "ls_printf(" + argument(0) + ", 0, (const LS_Value*)0)";
                const auto arguments = "ls_printf_arguments_" + std::to_string(nextLabel++);
                std::string result = "({ LS_Value " + arguments + "[" + std::to_string(count) + "]; ";
                for (size_t i{}; i < count; ++i) result += arguments + "[" + std::to_string(i) + "]=" + Box(Child(value, i + 2), owner) + "; ";
                return result + "ls_printf(" + argument(0) + ", " + std::to_string(count) + ", " + arguments + "); })";
            }
            if (types.IsFunction(callee.type)) {
                std::string arguments = "(const LS_Value*)0";
                std::string argumentSetup;
                if (childCount > 1) {
                    arguments = "ls_arguments_" + std::to_string(nextLabel++);
                    argumentSetup = "LS_Value " + arguments + "[" + std::to_string(childCount - 1) + "]; ";
                    for (size_t i = 1; i < childCount; ++i) argumentSetup += arguments + "[" + std::to_string(i - 1) + "]=" + Box(Child(value, i), owner) + "; ";
                }
                if (value.type == TypeTable::Void) return "({ " + argumentSetup + "ls_closure_call(" + Expression(callee, owner) + ", " + arguments + ", (LS_Value*)0); })";
                if (value.type == TypeTable::Any) return "({ " + argumentSetup + "LS_Value ls_result = {0}; ls_closure_call(" + Expression(callee, owner) + ", " + arguments + ", &ls_result); ls_result; })";
                const auto access = value.type == TypeTable::Int || IsEnum(value.type) ? "as.integer" : value.type == TypeTable::Float ? "as.number" : value.type == TypeTable::Bool ? "as.boolean" : "as.pointer";
                return "({ " + argumentSetup + "LS_Value ls_result = {0}; ls_closure_call(" + Expression(callee, owner) + ", " + arguments + ", &ls_result); ls_result." + access + "; })";
            }
            const auto receiver = [&] { return Expression(Child(callee, 0), owner); };
            switch (value.lowering) {
            case Lowering::CastIdentity: case Lowering::CastIntFloat: case Lowering::CastNumberBool: return "(" + Type(value.type) + ")(" + argument(0) + ")";
            case Lowering::CastBoolNumber: return "(" + argument(0) + " != 0)";
            case Lowering::CastBoolString: return "ls_string_truthy(" + argument(0) + ")";
            case Lowering::CastNumberString: throw std::runtime_error("Native C backend does not support parsing numeric strings yet");
            case Lowering::StringLength: return "ls_string_length(" + receiver() + ")";
            case Lowering::StringContains: return "ls_string_contains(" + receiver() + ", " + argument(0) + ")";
            case Lowering::StringStartsWith: return "ls_string_starts_with(" + receiver() + ", " + argument(0) + ")";
            case Lowering::StringEndsWith: return "ls_string_ends_with(" + receiver() + ", " + argument(0) + ")";
            case Lowering::CastString: case Lowering::StringSubstring: case Lowering::StringLower: case Lowering::StringUpper:
                throw std::runtime_error("Native C backend does not support allocating string operation '" + std::string(LoweringName(value.lowering)) + "' yet");
            default: break;
            }
            std::string result;
            size_t firstArgument = 1;
            if (callee.lowering == Lowering::ImplicitMethod) { result = Function(owner, callee.Text()) + "(self"; }
            else if (callee.kind == SyntaxExpression::Kind::Member && value.lowering != Lowering::StaticCall) {
                const auto& target = Child(callee, 0);
                result = Function(Name(target.type), callee.Text()) + "(" + Expression(target, owner);
            } else { result = Expression(callee, owner) + "("; firstArgument = 0; }
            for (size_t i = 1; i < childCount; ++i) {
                if (i != 1 || firstArgument == 1) result += ", ";
                result += child(i);
            }
            return result + ")";
        }
        case SyntaxExpression::Kind::Unary:
            if (text == "-" && value.type == TypeTable::Int) return "ls_int_neg(" + child(0) + ")";
            return "(" + text + child(0) + ")";
        case SyntaxExpression::Kind::Binary: {
            if (value.lowering == Lowering::Concat) throw std::runtime_error("Native C backend does not support allocating string concatenation yet");
            const auto left = Child(value, 0).type, right = Child(value, 1).type;
            if (left == TypeTable::Any || right == TypeTable::Any) {
                const auto boxedLeft = Box(Child(value, 0), owner), boxedRight = Box(Child(value, 1), owner);
                if (text == "==" || text == "!=") return "({ LS_Value ls_left=" + boxedLeft + ",ls_right=" + boxedRight + "; " + std::string(text == "!=" ? "!" : "") + "ls_value_equal(&ls_left,&ls_right); })";
                if (text == "<" || text == ">" || text == "<=" || text == ">=") {
                    const int operation = text == "<" ? 0 : text == "<=" ? 1 : text == ">" ? 2 : 3;
                    return "({ LS_Value ls_left=" + boxedLeft + ",ls_right=" + boxedRight + "; ls_value_compare(&ls_left," + std::to_string(operation) + ",&ls_right); })";
                }
                const int operation = text == "+" ? 0 : text == "-" ? 1 : text == "*" ? 2 : text == "/" ? 3 : text == "%" ? 4 : -1;
                if (operation >= 0) return "({ LS_Value ls_left=" + boxedLeft + ",ls_right=" + boxedRight + ",ls_result={0}; ls_value_arithmetic(&ls_left," + std::to_string(operation) + ",&ls_right,&ls_result); ls_result; })";
            }
            if ((text == "==" || text == "!=") && left == TypeTable::String)
                return std::string(text == "!=" ? "!" : "") + "ls_string_equal(" + child(0) + ", " + child(1) + ")";
            if (left == TypeTable::Int && right == TypeTable::Int) {
                if (text == "+") return "ls_int_add(" + child(0) + ", " + child(1) + ")";
                if (text == "-") return "ls_int_sub(" + child(0) + ", " + child(1) + ")";
                if (text == "*") return "ls_int_mul(" + child(0) + ", " + child(1) + ")";
                if (text == "/") return "ls_int_div(" + child(0) + ", " + child(1) + ")";
                if (text == "%") return "ls_int_mod(" + child(0) + ", " + child(1) + ")";
            }
            return "(" + child(0) + " " + text + " " + child(1) + ")";
        }
        case SyntaxExpression::Kind::Assignment: {
            const auto& target = Child(value, 0);
            if (target.kind == SyntaxExpression::Kind::Index && Child(target, 0).type == TypeTable::Table) {
                if (text != "=") throw std::runtime_error("Native C backend does not support compound table assignment yet");
                return "ls_table_set(" + Expression(Child(target, 0), owner) + ", &(" + Box(Child(target, 1), owner) + "), &(" + Box(Child(value, 1), owner) + "))";
            }
            if (value.lowering == Lowering::ListSet) throw std::runtime_error("Native C backend does not support list assignment yet");
            if (target.type == TypeTable::Int && text.size() == 2) {
                const auto operation = text.front() == '+' ? "ls_int_add" : text.front() == '-' ? "ls_int_sub" : text.front() == '*' ? "ls_int_mul" : "";
                if (*operation) return "(" + child(0) + " = " + operation + "(" + child(0) + ", " + child(1) + "))";
            }
            return "(" + child(0) + " " + text + " " + child(1) + ")";
        }
        case SyntaxExpression::Kind::Table: {
            if (!childCount) return "ls_table_create(0, (const LS_Value*)0)";
            const auto temporary = "ls_literal_" + std::to_string(nextLabel++);
            std::string result = "({ ls_table " + temporary + " = ls_table_create(0, (const LS_Value*)0); ";
            for (size_t i{}; i < childCount; ++i) {
                const auto& entry = Child(value, i);
                std::string key;
                std::string item;
                if (entry.kind == SyntaxExpression::Kind::TableEntry && !entry.Text().empty()) {
                    key = "((LS_Value){.kind=4,.as.pointer=ls_intern(\"" + entry.Text() + "\")})";
                    item = Box(Child(entry, 0), owner);
                } else if (entry.kind == SyntaxExpression::Kind::TableEntry) {
                    key = Box(Child(entry, 0), owner);
                    item = Box(Child(entry, 1), owner);
                } else {
                    key = "((LS_Value){.kind=1,.as.integer=" + std::to_string(i + 1) + "})";
                    item = Box(entry, owner);
                }
                result += "ls_table_set(" + temporary + ", &(" + key + "), &(" + item + ")); ";
            }
            return result + temporary + "; })";
        }
        case SyntaxExpression::Kind::Index:
            if (Child(value, 0).type == TypeTable::Table) throw std::runtime_error("Native table reads are currently only supported in variable initializers");
            throw std::runtime_error("Native C backend does not support indexing type '" + Name(Child(value, 0).type) + "'");
        case SyntaxExpression::Kind::Lambda: {
            const auto found = std::find(lambdas.begin(), lambdas.end(), &value);
            if (found == lambdas.end()) throw std::runtime_error("Native closure metadata was not collected");
            const auto index = static_cast<size_t>(std::distance(lambdas.begin(), found));
            if (lambdaCaptures[index].empty()) return "ls_closure_create(ls_lambda_" + std::to_string(index) + ", (void*)0)";
            const auto temporary = "ls_environment_" + std::to_string(nextLabel++);
            std::string result = "({ LS_LambdaEnv_" + std::to_string(index) + "* " + temporary + "=(LS_LambdaEnv_" + std::to_string(index) + "*)ls_alloc(sizeof(LS_LambdaEnv_" + std::to_string(index) + "),ls_trace_lambda_env_" + std::to_string(index) + "); ";
            for (const auto* capture : lambdaCaptures[index]) result += temporary + "->" + Identifier(capture->name) + "=" + (capture->name == "this" ? "self" : Identifier(capture->name)) + "; ";
            return result + "ls_closure_create(ls_lambda_" + std::to_string(index) + ", " + temporary + "); })";
        }
        case SyntaxExpression::Kind::TableEntry:
        case SyntaxExpression::Kind::Slice: case SyntaxExpression::Kind::Range:
            throw std::runtime_error("Native C backend does not support this expression kind yet (type '" + Name(value.type) + "')");
        }
        throw std::runtime_error("Native C backend encountered an unknown expression");
    }

    void Statements(NodeRange statements, std::string_view indent, std::string_view owner = {}, std::string_view continueLabel = {}, bool moduleTopLevel = false) {
        const std::string inner = std::string(indent) + "    ", innermost = inner + "    ";
        for (const auto& statement : program.Statements(statements)) {
            const auto& expression = program.Expression(statement.expression);
            switch (statement.kind) {
            case SyntaxStatement::Kind::Local:
            {
                const auto target = moduleTopLevel ? "ls_state." + Identifier(statement.Name()) : Identifier(statement.Name());
                const auto declaration = moduleTopLevel ? std::string{} : Type(statement.type) + " ";
                if (statement.type == TypeTable::Any && expression.kind == SyntaxExpression::Kind::Index && Child(expression, 0).type == TypeTable::Table) {
                    Line(indent, declaration + target + " = (LS_Value){0};");
                    const auto key = Box(Child(expression, 1), owner);
                    Line(indent, "ls_table_get(" + Expression(Child(expression, 0), owner) + ", &(" + key + "), &" + target + ");");
                } else Line(indent, declaration + target + " = " + (expression.kind == SyntaxExpression::Kind::Missing ? Default(Name(statement.type)) : Expression(expression, owner)) + ";");
                break;
            }
            case SyntaxStatement::Kind::Expression: Line(indent, Expression(expression, owner) + ";"); break;
            case SyntaxStatement::Kind::Return: Line(indent, expression.kind == SyntaxExpression::Kind::Missing ? "return;" : "return " + Expression(expression, owner) + ";"); break;
            case SyntaxStatement::Kind::Break: Line(indent, "break;"); break;
            case SyntaxStatement::Kind::Continue: Line(indent, continueLabel.empty() ? "continue;" : "goto " + std::string(continueLabel) + ";"); break;
            case SyntaxStatement::Kind::If:
                Line(indent, "if (" + Expression(expression, owner) + ") {"); Statements(statement.body, inner, owner, continueLabel); Line(indent, "}");
                if (statement.elseBody.count) { Line(indent, "else {"); Statements(statement.elseBody, inner, owner, continueLabel); Line(indent, "}"); } break;
            case SyntaxStatement::Kind::While:
                Line(indent, "while (" + Expression(expression, owner) + ") {"); Statements(statement.body, inner, owner); Line(indent, "}"); break;
            case SyntaxStatement::Kind::For:
            {
                const auto label = "ls_continue_" + std::to_string(nextLabel++);
                const auto& increment = program.Expression(statement.increment);
                Line(indent, "{"); Statements(statement.initializer, inner, owner);
                Line(inner, "while (" + Expression(expression, owner) + ") {");
                Statements(statement.body, innermost, owner, label);
                Line(innermost, label + ":;");
                if (increment.kind != SyntaxExpression::Kind::Missing) Line(innermost, Expression(increment, owner) + ";");
                Line(inner, "}"); Line(indent, "}"); break;
            }
            case SyntaxStatement::Kind::ForEach:
                throw std::runtime_error("Native C backend does not support foreach/range loops yet");
            }
        }
    }

    std::string Parameters(const BoundFunction& function) const {
        std::string result;
        for (size_t i{}; i < function.parameters.size(); ++i) {
            if (!result.empty()) result += ", ";
            result += Type(function.parameters[i].type) + " " + Identifier(function.parameters[i].name);
        }
        return result.empty() ? "void" : result;
    }

    std::string MethodParameters(const BoundClass& owner, const BoundFunction& function) const {
        std::string result = "LS_" + Identifier(owner.name) + "* self";
        for (const auto& parameter : function.parameters) result += ", " + Type(parameter.type) + " " + Identifier(parameter.name);
        return result;
    }

    CCompilationResult Run() {
        Line({}, "/* generated by LuaSharp; C11 */");
        Line({}, "typedef signed long long ls_int; /* public LuaSharp int: signed 64-bit */"); Line({}, "typedef unsigned long long ls_wrap_bits; /* internal arithmetic bits; never a LuaSharp value */"); Line({}, "typedef double ls_float;"); Line({}, "typedef unsigned char ls_bool;");
        Line({}, "typedef struct LS_String { ls_int length; char data[1]; } LS_String;");
        Line({}, "typedef LS_String* ls_string;");
        Line({}, "typedef void LS_Table; typedef LS_Table* ls_table;");
        Line({}, "typedef struct LS_Value { ls_wrap_bits kind; union { ls_int integer; ls_float number; ls_wrap_bits boolean; void* pointer; } as; } LS_Value;");
        Line({}, "typedef struct LS_Closure { void (*invoke)(void* environment, const LS_Value* arguments, LS_Value* result); void* environment; } LS_Closure;");
        Line({}, "typedef void (*LS_TraceFunction)(void* runtime_context, void* object);");
        Line({}, "typedef struct LS_RuntimeApi { ls_wrap_bits abi_version; void* context; void* (*allocate)(void* context, ls_wrap_bits size, LS_TraceFunction trace); void (*mark)(void* context, void* object); void* (*intern)(void* context, const char* value); void* (*table_create)(void* context, ls_wrap_bits count, const LS_Value* pairs); void (*table_get)(void* context, void* table, const LS_Value* key, LS_Value* out); void (*table_set)(void* context, void* table, const LS_Value* key, const LS_Value* value); void (*print)(void* context, void* format, ls_wrap_bits count, const LS_Value* arguments); void (*report_error)(void* context, const char* message); } LS_RuntimeApi;");
        Line({}, "static LS_RuntimeApi* ls_runtime;");
        Line({}, "static void ls_report_error(const char* message) { if (ls_runtime && ls_runtime->report_error) ls_runtime->report_error(ls_runtime->context, message); }");
        Line({}, "static void* ls_alloc(ls_wrap_bits size, LS_TraceFunction trace) { void* value; if (!ls_runtime || !ls_runtime->allocate) return (void*)0; value = ls_runtime->allocate(ls_runtime->context, size, trace); if (!value) ls_report_error(\"LuaSharp allocation failed\"); return value; }");
        Line({}, "static ls_string ls_intern(const char* value) { return ls_runtime && ls_runtime->intern ? (ls_string)ls_runtime->intern(ls_runtime->context, value) : (ls_string)0; }");
        Line({}, "static void ls_mark_value(void* context, const LS_Value* value) { if (value && value->kind >= 4 && value->kind <= 7) ls_runtime->mark(context, value->as.pointer); }");
        Line({}, "static void ls_trace_closure(void* context, void* object) { LS_Closure* closure=(LS_Closure*)object; if(closure->environment) ls_runtime->mark(context, closure->environment); }");
        Line({}, "static LS_Closure* ls_closure_create(void (*invoke)(void*, const LS_Value*, LS_Value*), void* environment) { LS_Closure* closure=(LS_Closure*)ls_alloc(sizeof(LS_Closure), ls_trace_closure); if(!closure)return (LS_Closure*)0; closure->invoke=invoke; closure->environment=environment; return closure; }");
        Line({}, "static void ls_closure_call(LS_Closure* closure, const LS_Value* arguments, LS_Value* result) { if(!closure||!closure->invoke){ls_report_error(\"attempt to call null closure\");return;} closure->invoke(closure->environment,arguments,result); }");
        Line({}, "static ls_table ls_table_create(ls_wrap_bits count, const LS_Value* pairs) { return ls_runtime && ls_runtime->table_create ? (ls_table)ls_runtime->table_create(ls_runtime->context, count, pairs) : (ls_table)0; }");
        Line({}, "static void ls_table_get(ls_table table, const LS_Value* key, LS_Value* out) { if (ls_runtime && ls_runtime->table_get) ls_runtime->table_get(ls_runtime->context, table, key, out); }");
        Line({}, "static void ls_table_set(ls_table table, const LS_Value* key, const LS_Value* value) { if (ls_runtime && ls_runtime->table_set) ls_runtime->table_set(ls_runtime->context, table, key, value); }");
        Line({}, "static void ls_printf(ls_string format, ls_wrap_bits count, const LS_Value* arguments) { if (ls_runtime && ls_runtime->print) ls_runtime->print(ls_runtime->context, format, count, arguments); }");
        Line({}, "static ls_int ls_int_add(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a + (ls_wrap_bits)b); }");
        Line({}, "static ls_int ls_int_sub(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a - (ls_wrap_bits)b); }");
        Line({}, "static ls_int ls_int_mul(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a * (ls_wrap_bits)b); }");
        Line({}, "static ls_int ls_int_neg(ls_int a) { return (ls_int)(0ULL - (ls_wrap_bits)a); }");
        Line({}, "static ls_float ls_int_div(ls_int a, ls_int b) { return (ls_float)a / (ls_float)b; }");
        Line({}, "static ls_int ls_int_mod(ls_int a, ls_int b) { ls_int r; if (!b) { ls_report_error(\"integer remainder by zero\"); return 0; } if (b == -1) return 0; r = a % b; if (r && ((r < 0) != (b < 0))) r = ls_int_add(r, b); return r; }");
        Line({}, "static ls_bool ls_value_equal(const LS_Value* a, const LS_Value* b) { if(a->kind==1&&b->kind==2)return (ls_float)a->as.integer==b->as.number; if(a->kind==2&&b->kind==1)return a->as.number==(ls_float)b->as.integer; if(a->kind!=b->kind)return 0; if(a->kind==0)return 1; if(a->kind==1)return a->as.integer==b->as.integer; if(a->kind==2)return a->as.number==b->as.number; if(a->kind==3)return a->as.boolean==b->as.boolean; return a->as.pointer==b->as.pointer; }");
        Line({}, "static ls_bool ls_value_compare(const LS_Value* a, int operation, const LS_Value* b) { ls_float left,right; if((a->kind!=1&&a->kind!=2)||(b->kind!=1&&b->kind!=2)){ls_report_error(\"dynamic comparison requires numbers\");return 0;} left=a->kind==1?(ls_float)a->as.integer:a->as.number; right=b->kind==1?(ls_float)b->as.integer:b->as.number; return operation==0?left<right:operation==1?left<=right:operation==2?left>right:left>=right; }");
        Line({}, "static void ls_value_arithmetic(const LS_Value* a, int operation, const LS_Value* b, LS_Value* out) { if((a->kind!=1&&a->kind!=2)||(b->kind!=1&&b->kind!=2)){ls_report_error(\"dynamic arithmetic requires numbers\");*out=(LS_Value){0};return;} if(operation==4){if(a->kind!=1||b->kind!=1){ls_report_error(\"dynamic remainder requires integers\");*out=(LS_Value){0};return;} out->kind=1;out->as.integer=ls_int_mod(a->as.integer,b->as.integer);return;} if(operation==3||a->kind==2||b->kind==2){ls_float left=a->kind==1?(ls_float)a->as.integer:a->as.number,right=b->kind==1?(ls_float)b->as.integer:b->as.number;out->kind=2;out->as.number=operation==0?left+right:operation==1?left-right:operation==2?left*right:left/right;return;} out->kind=1;out->as.integer=operation==0?ls_int_add(a->as.integer,b->as.integer):operation==1?ls_int_sub(a->as.integer,b->as.integer):ls_int_mul(a->as.integer,b->as.integer); }");
        Line({}, "static ls_int ls_string_length(ls_string s) { return s ? s->length : 0; }");
        Line({}, "static ls_bool ls_string_starts_with(ls_string s, ls_string p) { ls_int i; if (!s || !p || p->length > s->length) return 0; for (i=0;i<p->length;++i) if (s->data[i]!=p->data[i]) return 0; return 1; }");
        Line({}, "static ls_bool ls_string_ends_with(ls_string s, ls_string p) { ls_int i,offset; if (!s || !p || p->length > s->length) return 0; offset=s->length-p->length; for(i=0;i<p->length;++i) if(s->data[offset+i]!=p->data[i]) return 0; return 1; }");
        Line({}, "static ls_bool ls_string_contains(ls_string s, ls_string p) { ls_int i,j; if(!s||!p)return 0; if(!p->length)return 1; for(i=0;i+p->length<=s->length;++i){for(j=0;j<p->length&&s->data[i+j]==p->data[j];++j){} if(j==p->length)return 1;} return 0; }");
        Line({}, "static ls_bool ls_string_equal(ls_string a, ls_string b) { return a==b; }");
        Line({}, "static ls_bool ls_string_truthy(ls_string s) { return s && s->length != 0; }");
        Line({});
        for (const auto& value : program.classes) Line({}, "typedef struct LS_" + Identifier(value.name) + " LS_" + Identifier(value.name) + ";");
        for (const auto& value : externalTypes) if (!classes.contains(value.name) && !value.isEnum) Line({}, "typedef struct LS_" + Identifier(value.name) + " LS_" + Identifier(value.name) + ";");
        if (!program.classes.empty() || !externalTypes.empty()) Line({});
        for (const auto& value : externalTypes) if (!value.isEnum) for (const auto& member : value.members) if (member.kind == SymbolKind::Method) {
            std::string parameters = "LS_" + Identifier(value.name) + "* self";
            for (const auto& parameter : member.parameters) {
                const auto split = parameter.find(' ');
                auto parameterType = parameter.substr(0, split);
                if (parameterType.ends_with('?')) parameterType.pop_back();
                const auto parameterName = split == std::string::npos ? "argument" : parameter.substr(split + 1);
                parameters += ", " + Type(parameterType) + " " + Identifier(parameterName);
            }
            Line({}, "extern " + Type(member.type) + " " + Function(value.name, member.name) + "(" + parameters + ");");
        }
        if (!externalTypes.empty()) Line({});
        for (const auto& value : program.enums) {
            for (const auto& member : value.members) Line({}, "#define LS_" + Identifier(value.name) + "_" + Identifier(member.name) + " ((ls_int)" + std::to_string(member.value) + ")");
        }
        if (!program.enums.empty()) Line({});
        for (const auto& value : program.classes) {
            if (!value.baseName.empty()) throw std::runtime_error("Native C backend does not support class inheritance yet ('" + value.name + "')");
            Line({}, "struct LS_" + Identifier(value.name) + " {");
            if (!value.baseName.empty()) Line("    ", "LS_" + Identifier(value.baseName) + " base;");
            for (const auto& field : value.fields) Line("    ", Type(field.type) + " " + Identifier(field.name) + ";");
            if (value.fields.empty() && value.baseName.empty()) Line("    ", "unsigned char _unused;");
            Line({}, "};");
        }
        for (size_t i{}; i < lambdas.size(); ++i) if (!lambdaCaptures[i].empty()) {
            Line({}, "typedef struct LS_LambdaEnv_" + std::to_string(i) + " {");
            for (const auto* capture : lambdaCaptures[i]) Line("    ", Type(capture->type) + " " + Identifier(capture->name) + ";");
            Line({}, "} LS_LambdaEnv_" + std::to_string(i) + ";");
            Line({}, "static void ls_trace_lambda_env_" + std::to_string(i) + "(void* runtime_context, void* object) {");
            Line("    ", "LS_LambdaEnv_" + std::to_string(i) + "* ls_env=(LS_LambdaEnv_" + std::to_string(i) + "*)object;");
            bool traced = false;
            for (const auto* capture : lambdaCaptures[i]) if (IsManagedObject(Name(capture->type))) {
                traced = true;
                if (capture->type == TypeTable::Any) Line("    ", "ls_mark_value(runtime_context, &ls_env->" + Identifier(capture->name) + ");");
                else Line("    ", "ls_runtime->mark(runtime_context, ls_env->" + Identifier(capture->name) + ");");
            }
            if (!traced) Line("    ", "(void)runtime_context; (void)ls_env;");
            Line({}, "}");
        }
        if (!program.classes.empty()) Line({});
        for (const auto& value : program.classes) {
            Line({}, "static void ls_trace_" + Identifier(value.name) + "(void* runtime_context, void* object) {");
            Line("    ", "LS_" + Identifier(value.name) + "* self = (LS_" + Identifier(value.name) + "*)object;");
            bool tracesField = false;
            for (const auto& field : value.fields) if (IsManagedObject(field.type)) {
                tracesField = true;
                if (field.type == "any") Line("    ", "ls_mark_value(runtime_context, &self->" + Identifier(field.name) + ");");
                else Line("    ", "ls_runtime->mark(runtime_context, self->" + Identifier(field.name) + ");");
            }
            if (!tracesField) Line("    ", "(void)runtime_context; (void)self;");
            Line({}, "}");
        }
        Line({}, "typedef struct LS_ModuleState {");
        bool hasGlobals = false;
        for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local) {
            hasGlobals = true; Line("    ", Type(statement.type) + " " + Identifier(statement.Name()) + ";");
        }
        if (!hasGlobals) Line("    ", "unsigned char _unused;");
        Line({}, "} LS_ModuleState;");
        Line({}, "static LS_ModuleState ls_state;");
        Line({}, "void ls_module_trace_roots(void* runtime_context) {");
        bool tracesGlobal = false;
        for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local && IsManagedObject(Name(statement.type))) {
            tracesGlobal = true;
            if (statement.type == TypeTable::Any) Line("    ", "ls_mark_value(runtime_context, &ls_state." + Identifier(statement.Name()) + ");");
            else Line("    ", "ls_runtime->mark(runtime_context, ls_state." + Identifier(statement.Name()) + ");");
        }
        if (!tracesGlobal) Line("    ", "(void)runtime_context;");
        Line({}, "}");
        Line({});
        for (size_t i{}; i < lambdas.size(); ++i) Line({}, "static void ls_lambda_" + std::to_string(i) + "(void* environment, const LS_Value* arguments, LS_Value* result);");
        if (!lambdas.empty()) Line({});
        CCompilationResult result;
        for (const auto& value : program.classes) {
            NativeTypeLayout layout; layout.name = value.name; layout.baseName = value.baseName;
            for (const auto& field : value.fields) layout.fields.push_back({field.name, field.type});
            result.types.push_back(std::move(layout));
        }
        Line({}, "ls_int ls_module_abi_version(void) { return 4; }");
        Line({}, "int ls_module_bind(LS_RuntimeApi* runtime) { if (!runtime || runtime->abi_version != 4 || !runtime->allocate || !runtime->mark || !runtime->intern || !runtime->table_create || !runtime->table_get || !runtime->table_set || !runtime->print || !runtime->report_error) return -1; ls_runtime = runtime; return 0; }");
        result.exports.push_back({"$abi_version", "ls_module_abi_version", "int", {}});
        result.exports.push_back({"$module_bind", "ls_module_bind", "int", {"$runtime_api"}});
        for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local) {
            const auto symbol = "ls_global_" + Identifier(statement.Name());
            Line({}, "void* " + symbol + "(void) { return &ls_state." + Identifier(statement.Name()) + "; }");
            result.globals.push_back({statement.Name(), symbol, Name(statement.type)});
        }
        for (const auto& value : program.classes) {
            const auto constructor = std::find_if(value.functions.begin(), value.functions.end(), [](const auto& item) { return item.isConstructor; });
            std::string ctorParameters = "void";
            if (constructor != value.functions.end()) ctorParameters = Parameters(*constructor);
            Line({}, "LS_" + Identifier(value.name) + "* " + Function(value.name, "new") + "(" + ctorParameters + ");");
            for (const auto& function : value.functions) if (!function.isConstructor)
                Line({}, Type(function.returnType) + " " + Function(value.name, function.name) + "(" + MethodParameters(value, function) + ");");
        }
        Line({});
        for (const auto& value : program.classes) {
            const auto constructor = std::find_if(value.functions.begin(), value.functions.end(), [](const auto& item) { return item.isConstructor; });
            std::string ctorParameters = "void";
            if (constructor != value.functions.end()) ctorParameters = Parameters(*constructor);
            Line({}, "LS_" + Identifier(value.name) + "* " + Function(value.name, "new") + "(" + ctorParameters + ") {");
            Line("    ", "LS_" + Identifier(value.name) + "* self = (LS_" + Identifier(value.name) + "*)ls_alloc(sizeof(LS_" + Identifier(value.name) + "), ls_trace_" + Identifier(value.name) + ");");
            Line("    ", "if (!self) return (LS_" + Identifier(value.name) + "*)0;");
            for (const auto& field : value.fields) Line("    ", "self->" + Identifier(field.name) + " = " + (program.Expression(field.initializer).kind == SyntaxExpression::Kind::Missing ? Default(field.type) : Expression(program.Expression(field.initializer), value.name)) + ";");
            if (constructor != value.functions.end()) Statements(constructor->body, "    ", value.name);
            Line("    ", "return self;"); Line({}, "}");
            result.exports.push_back({value.name + ".new", Function(value.name, "new"), value.name, constructor == value.functions.end() ? std::vector<std::string>{} : [&] { std::vector<std::string> p; for (const auto& v : constructor->parameters) p.push_back(v.type); return p; }()});
            for (const auto& function : value.functions) {
                if (function.isConstructor) continue;
                Line({}, Type(function.returnType) + " " + Function(value.name, function.name) + "(" + MethodParameters(value, function) + ") {");
                Statements(function.body, "    ", value.name); Line({}, "}");
                std::vector<std::string> parameters{value.name}; for (const auto& p : function.parameters) parameters.push_back(p.type);
                result.exports.push_back({value.name + "." + function.name, Function(value.name, function.name), function.returnType, std::move(parameters)});
            }
        }
        for (size_t i{}; i < lambdas.size(); ++i) {
            const auto& lambda = *lambdas[i];
            Line({}, "static void ls_lambda_" + std::to_string(i) + "(void* environment, const LS_Value* arguments, LS_Value* result) {");
            if (lambdaCaptures[i].empty()) Line("    ", "(void)environment;");
            else {
                Line("    ", "LS_LambdaEnv_" + std::to_string(i) + "* ls_env=(LS_LambdaEnv_" + std::to_string(i) + "*)environment;");
                const auto self = std::find_if(lambdaCaptures[i].begin(), lambdaCaptures[i].end(), [](const auto* value) { return value->name == "this"; });
                if (self != lambdaCaptures[i].end()) Line("    ", Type((*self)->type) + " self=ls_env->" + Identifier((*self)->name) + ";");
            }
            Line("    ", "(void)arguments;");
            for (size_t parameter{}; parameter < lambda.Parameters().size(); ++parameter)
                Line("    ", "LS_Value " + Identifier(lambda.Parameters()[parameter].name) + "=arguments[" + std::to_string(parameter) + "];");
            const auto returnType = types.Element(lambda.type);
            activeLambda = i;
            if (lambda.LambdaIsBlock()) {
                if (returnType != TypeTable::Void) throw std::runtime_error("Native block closures returning values are not supported yet");
                Line("    ", "(void)result;");
                Statements(lambda.lambdaBody, "    ");
            } else {
                if (returnType == TypeTable::Void) { Line("    ", "(void)result;"); Expression(Child(lambda, 0)); }
                else Line("    ", "if(result) *result = " + Box(Child(lambda, 0)) + ";");
            }
            activeLambda.reset();
            Line({}, "}");
        }
        if (!lambdas.empty()) Line({});
        Line({}, "int ls_module_init(void) {"); Statements(program.topLevel, "    ", {}, {}, true); Line("    ", "return 0;"); Line({}, "}");
        result.exports.push_back({"$module_init", "ls_module_init", "int", {}});
        result.c = std::move(output); return result;
    }
};

std::runtime_error SyntaxError(const SyntaxDiagnostic& diagnostic) { return std::runtime_error(diagnostic.message + " at " + std::to_string(diagnostic.span.offset)); }

} // namespace

CCompilationResult CompileToC(std::string_view source) {
    return CompileToC(source, {});
}

CCompilationResult CompileToC(std::string_view source, const std::vector<ExternalType>& externalTypes) {
    return CompileToC(Analyze(source, externalTypes), externalTypes);
}

CCompilationResult CompileToC(const Analysis& analysis, const std::vector<ExternalType>& externalTypes) {
    if (!analysis.syntax.diagnostics.empty()) throw SyntaxError(analysis.syntax.diagnostics.front());
    const auto error = std::find_if(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& value) { return value.severity == DiagnosticSeverity::Error; });
    if (error != analysis.diagnostics.end()) throw std::runtime_error(error->message + " at " + std::to_string(error->span.offset));
    return CEmitter(analysis, externalTypes).Run();
}

} // namespace luasharp
