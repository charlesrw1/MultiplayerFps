#include "CBackend.h"

#include "Builtins.h"
#include "Semantic.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace luasharp {
namespace {

constexpr bool IsIdentifierCharacter(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') || value == '_';
}

void AppendIdentifier(std::string& out, std::string_view value) {
    if (value.empty() || (value.front() >= '0' && value.front() <= '9')) out += '_';
    if (std::all_of(value.begin(), value.end(), IsIdentifierCharacter)) { out += value; return; }
    for (const char character : value) out += IsIdentifierCharacter(character) ? character : '_';
}

std::string Identifier(std::string_view value) {
    std::string result;
    AppendIdentifier(result, value);
    return result;
}

void AppendFunction(std::string& out, std::string_view owner, std::string_view name) {
    out += "ls_"; AppendIdentifier(out, owner); out += '_'; AppendIdentifier(out, name);
}

std::string FunctionName(std::string_view owner, std::string_view name) {
    std::string result;
    AppendFunction(result, owner, name);
    return result;
}

std::optional<std::string_view> ScalarCType(std::string_view type, const StringSet& enums) {
    if (type == "void") return "void";
    if (type == "int" || enums.contains(type)) return "ls_int";
    if (type == "float") return "ls_float";
    if (type == "bool") return "ls_bool";
    if (type == "string") return "ls_string";
    if (type == "table") return "ls_table";
    if (type == "any") return "LS_Value";
    if (type == "null") return "void*";
    if (type.starts_with("$function:")) return "LS_Closure*";
    return std::nullopt;
}

void AppendCType(std::string& out, std::string_view type, const StringSet& enums) {
    if (const auto scalar = ScalarCType(type, enums)) { out += *scalar; return; }
    if (type.ends_with("[]"))
        throw std::runtime_error("Native C backend does not support type '" + std::string(type) + "' yet");
    if (IsBuiltinType(type)) throw std::runtime_error("Native C backend requires a registered ABI for builtin type '" + std::string(type) + "'");
    out += "LS_"; AppendIdentifier(out, type); out += '*';
}

// Output fragments that Put() appends in place.
struct Id { std::string_view value; };                         // sanitized C identifier
struct Fn { std::string_view owner; std::string_view name; };  // ls_<owner>_<name>
struct CTypeName { std::string_view type; };                   // C spelling of a LuaSharp type

struct CEmitter {
    const Analysis& analysis;
    const BoundProgram& program;
    const TypeTable& types;
    const std::vector<ExternalType>& externalTypes;
    StringSet enums;
    StringSet externalNames;
    StringMap<const BoundClass*> classes;
    std::unordered_set<size_t> globalSymbols;
    std::vector<const BoundExpression*> lambdas;
    std::unordered_map<const BoundExpression*, size_t> lambdaIndices;
    std::vector<std::vector<const VariableBinding*>> lambdaCaptures;
    std::optional<size_t> activeLambda;
    std::string output;
    size_t nextLabel{};

    CEmitter(const Analysis& value, const std::vector<ExternalType>& external) : analysis(value), program(value.bound), types(value.types), externalTypes(external) {
        for (const auto& item : program.enums) enums.insert(item.name);
        for (const auto& item : program.classes) classes[item.name] = &item;
        for (const auto& item : externalTypes) { externalNames.insert(item.name); if (item.isEnum) enums.insert(item.name); }
        std::vector<const VariableBinding*> moduleBindings;
        for (const auto& binding : analysis.bindings) if (binding.depth == 0) moduleBindings.push_back(&binding);
        for (const auto& statement : program.Statements(program.topLevel)) {
            if (statement.kind != SyntaxStatement::Kind::Local) continue;
            for (const auto* binding : moduleBindings) {
                if (binding->name == statement.Name() && binding->declaration.offset >= statement.span.offset && binding->declaration.offset <= statement.span.End()) {
                    globalSymbols.insert(binding->symbolIndex); break;
                }
            }
        }
        const auto expression = [&](auto&& self, const BoundExpression& value) -> void {
            if (value.kind == SyntaxExpression::Kind::Lambda) { lambdaIndices.emplace(&value, lambdas.size()); lambdas.push_back(&value); }
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
        std::vector<const VariableBinding*> bindingsBySymbol; // first binding of each symbol index
        if (!lambdas.empty()) {
            bindingsBySymbol.resize(analysis.symbols.size());
            for (const auto& binding : analysis.bindings) {
                assert(binding.symbolIndex < bindingsBySymbol.size());
                if (!bindingsBySymbol[binding.symbolIndex]) bindingsBySymbol[binding.symbolIndex] = &binding;
            }
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
                    assert(*value.symbolIndex < bindingsBySymbol.size());
                    const auto* binding = bindingsBySymbol[*value.symbolIndex];
                    if (binding && (binding->declaration.offset < lambda->span.offset || binding->declaration.offset >= lambda->span.End())) add(binding);
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

    // Appends each part to output. Invocable parts emit themselves.
    template <typename... Parts> void Put(const Parts&... parts) { (Emit(parts), ...); }
    template <typename... Parts> void Line(size_t depth, const Parts&... parts) { output.append(depth * 4, ' '); Put(parts...); output += '\n'; }
    template <typename T> void Emit(const T& part) {
        static_assert(!std::is_same_v<T, bool>);
        if constexpr (std::is_invocable_v<const T&>) part();
        else if constexpr (std::is_same_v<T, Id>) AppendIdentifier(output, part.value);
        else if constexpr (std::is_same_v<T, Fn>) AppendFunction(output, part.owner, part.name);
        else if constexpr (std::is_same_v<T, CTypeName>) AppendCType(output, part.type, enums);
        else if constexpr (std::is_same_v<T, char>) output += part;
        else if constexpr (std::is_integral_v<T>) { char buffer[24]; output.append(buffer, std::to_chars(buffer, std::end(buffer), part).ptr); }
        else output.append(std::string_view(part));
    }
    auto Emitted(const BoundExpression& value, std::string_view owner) { return [this, &value, owner] { Expression(value, owner); }; }
    auto Boxed(const BoundExpression& value, std::string_view owner) { return [this, &value, owner] { Box(value, owner); }; }

    const std::string& Name(TypeId type) const { return types.Name(type); }
    CTypeName Type(TypeId type) const { return {Name(type)}; }
    bool IsEnum(TypeId type) const { return enums.contains(Name(type)); }
    bool IsManagedObject(std::string_view type) const { return type == "string" || type == "table" || type == "any" || type.starts_with("$function:") || classes.contains(type); }
    const BoundExpression& Child(const BoundExpression& value, size_t index) const { return program.Child(value, index); }
    const Symbol* ExternalMember(std::string_view owner, std::string_view name) const {
        const auto type = std::find_if(externalTypes.begin(), externalTypes.end(), [&](const auto& value) { return value.name == owner; });
        if (type == externalTypes.end()) return nullptr;
        const auto member = std::find_if(type->members.begin(), type->members.end(), [&](const auto& value) { return value.name == name; });
        return member == type->members.end() ? nullptr : &*member;
    }

    void Default(std::string_view type) {
        if (type == "float") return Put("0.0");
        if (type == "string") return Put("ls_intern(\"\")");
        if (type == "table") return Put("((ls_table)0)");
        if (type == "any") return Put("((LS_Value){0})");
        if (type.starts_with("$function:")) return Put("((LS_Closure*)0)");
        if (type == "int" || type == "bool" || enums.contains(type)) return Put('0');
        if (!type.empty() && !IsBuiltinType(type)) return Put("((", CTypeName{type}, ")0)");
        throw std::runtime_error("Native C backend cannot default-initialize type '" + std::string(type) + "'");
    }

    void Box(const BoundExpression& value, std::string_view owner = {}) {
        const auto type = value.type;
        const auto boxed = [&](std::string_view prefix) { Put(prefix, Emitted(value, owner), ")})"); };
        if (type == TypeTable::Any) return Expression(value, owner);
        if (type == TypeTable::Null) return Put("((LS_Value){.kind=0})");
        if (type == TypeTable::Int || IsEnum(type)) return boxed("((LS_Value){.kind=1,.as.integer=(");
        if (type == TypeTable::Float) return boxed("((LS_Value){.kind=2,.as.number=(");
        if (type == TypeTable::Bool) return boxed("((LS_Value){.kind=3,.as.boolean=(");
        if (type == TypeTable::String) return boxed("((LS_Value){.kind=4,.as.pointer=(");
        if (type == TypeTable::Table) return boxed("((LS_Value){.kind=6,.as.pointer=(");
        if (types.IsFunction(type)) return boxed("((LS_Value){.kind=7,.as.pointer=(");
        if (classes.contains(Name(type))) return boxed("((LS_Value){.kind=5,.as.pointer=(");
        throw std::runtime_error("Native C backend cannot box type '" + Name(type) + "'");
    }

    void Expression(const BoundExpression& value, std::string_view owner = {}) {
        const auto child = [&](size_t index) { return Emitted(Child(value, index), owner); };
        const auto& text = value.Text();
        const size_t childCount = value.children.count;
        switch (value.kind) {
        case SyntaxExpression::Kind::Missing: return;
        case SyntaxExpression::Kind::Identifier:
            if (text == "this") return Put("self");
            if (value.lowering == Lowering::ImplicitField) return Put("self->", Id{text});
            if (value.symbolIndex && globalSymbols.contains(*value.symbolIndex)) return Put("ls_state.", Id{text});
            if (activeLambda && value.symbolIndex) for (const auto* capture : lambdaCaptures.at(*activeLambda)) if (capture->symbolIndex == *value.symbolIndex)
                return Put("ls_env->", Id{capture->name});
            return Put(Id{text});
        case SyntaxExpression::Kind::Number: return Put(text);
        case SyntaxExpression::Kind::String: return Put("ls_intern(", text, ')');
        case SyntaxExpression::Kind::Boolean: return Put(text == "true" ? '1' : '0');
        case SyntaxExpression::Kind::Null: return Put("((void*)0)");
        case SyntaxExpression::Kind::Parenthesized: return Put('(', child(0), ')');
        case SyntaxExpression::Kind::New:
            Put(Fn{text, "new"}, '(');
            for (size_t i{}; i < childCount; ++i) { if (i) Put(", "); Expression(Child(value, i), owner); }
            return Put(')');
        case SyntaxExpression::Kind::Member: {
            const auto target = Child(value, 0).type;
            if (types.Kind(target) == TypeKind::Meta) return Put("LS_", Id{types.Target(target)}, '_', Id{text});
            if (externalNames.contains(Name(target))) if (const auto* symbol = ExternalMember(Name(target), text); symbol && symbol->kind == SymbolKind::Field)
                throw std::runtime_error("Native C backend cannot directly access field '" + text + "' on opaque external type '" + Name(target) + "'");
            return Put(child(0), "->", Id{text});
        }
        case SyntaxExpression::Kind::Call: return Call(value, owner);
        case SyntaxExpression::Kind::Unary:
            if (text == "-" && value.type == TypeTable::Int) return Put("ls_int_neg(", child(0), ')');
            return Put('(', text, child(0), ')');
        case SyntaxExpression::Kind::Binary: {
            if (value.lowering == Lowering::Concat) throw std::runtime_error("Native C backend does not support allocating string concatenation yet");
            const auto left = Child(value, 0).type, right = Child(value, 1).type;
            if (left == TypeTable::Any || right == TypeTable::Any) {
                const auto boxedLeft = Boxed(Child(value, 0), owner), boxedRight = Boxed(Child(value, 1), owner);
                if (text == "==" || text == "!=") return Put("({ LS_Value ls_left=", boxedLeft, ",ls_right=", boxedRight, "; ", text == "!=" ? "!" : "", "ls_value_equal(&ls_left,&ls_right); })");
                if (text == "<" || text == ">" || text == "<=" || text == ">=") {
                    const int operation = text == "<" ? 0 : text == "<=" ? 1 : text == ">" ? 2 : 3;
                    return Put("({ LS_Value ls_left=", boxedLeft, ",ls_right=", boxedRight, "; ls_value_compare(&ls_left,", operation, ",&ls_right); })");
                }
                const int operation = text == "+" ? 0 : text == "-" ? 1 : text == "*" ? 2 : text == "/" ? 3 : text == "%" ? 4 : -1;
                if (operation >= 0) return Put("({ LS_Value ls_left=", boxedLeft, ",ls_right=", boxedRight, ",ls_result={0}; ls_value_arithmetic(&ls_left,", operation, ",&ls_right,&ls_result); ls_result; })");
            }
            if ((text == "==" || text == "!=") && left == TypeTable::String)
                return Put(text == "!=" ? "!" : "", "ls_string_equal(", child(0), ", ", child(1), ')');
            if (left == TypeTable::Int && right == TypeTable::Int) {
                const char* function = text == "+" ? "ls_int_add" : text == "-" ? "ls_int_sub" : text == "*" ? "ls_int_mul" : text == "/" ? "ls_int_div" : text == "%" ? "ls_int_mod" : nullptr;
                if (function) return Put(function, '(', child(0), ", ", child(1), ')');
            }
            return Put('(', child(0), ' ', text, ' ', child(1), ')');
        }
        case SyntaxExpression::Kind::Assignment: {
            const auto& target = Child(value, 0);
            if (target.kind == SyntaxExpression::Kind::Index && Child(target, 0).type == TypeTable::Table) {
                if (text != "=") throw std::runtime_error("Native C backend does not support compound table assignment yet");
                return Put("ls_table_set(", Emitted(Child(target, 0), owner), ", &(", Boxed(Child(target, 1), owner), "), &(", Boxed(Child(value, 1), owner), "))");
            }
            if (value.lowering == Lowering::ListSet) throw std::runtime_error("Native C backend does not support list assignment yet");
            if (target.type == TypeTable::Int && text.size() == 2) {
                const auto operation = text.front() == '+' ? "ls_int_add" : text.front() == '-' ? "ls_int_sub" : text.front() == '*' ? "ls_int_mul" : "";
                if (*operation) return Put('(', child(0), " = ", operation, '(', child(0), ", ", child(1), "))");
            }
            return Put('(', child(0), ' ', text, ' ', child(1), ')');
        }
        case SyntaxExpression::Kind::Table: {
            if (!childCount) return Put("ls_table_create(0, (const LS_Value*)0)");
            const auto label = nextLabel++;
            const auto temporary = [&] { Put("ls_literal_", label); };
            Put("({ ls_table ", temporary, " = ls_table_create(0, (const LS_Value*)0); ");
            for (size_t i{}; i < childCount; ++i) {
                const auto& entry = Child(value, i);
                Put("ls_table_set(", temporary, ", &(");
                if (entry.kind == SyntaxExpression::Kind::TableEntry && !entry.Text().empty())
                    Put("((LS_Value){.kind=4,.as.pointer=ls_intern(\"", entry.Text(), "\")})), &(", Boxed(Child(entry, 0), owner));
                else if (entry.kind == SyntaxExpression::Kind::TableEntry)
                    Put(Boxed(Child(entry, 0), owner), "), &(", Boxed(Child(entry, 1), owner));
                else
                    Put("((LS_Value){.kind=1,.as.integer=", i + 1, "})), &(", Boxed(entry, owner));
                Put(")); ");
            }
            return Put(temporary, "; })");
        }
        case SyntaxExpression::Kind::Index:
            if (Child(value, 0).type == TypeTable::Table) throw std::runtime_error("Native table reads are currently only supported in variable initializers");
            throw std::runtime_error("Native C backend does not support indexing type '" + Name(Child(value, 0).type) + "'");
        case SyntaxExpression::Kind::Lambda: {
            const auto found = lambdaIndices.find(&value);
            if (found == lambdaIndices.end()) throw std::runtime_error("Native closure metadata was not collected");
            const auto index = found->second;
            if (lambdaCaptures[index].empty()) return Put("ls_closure_create(ls_lambda_", index, ", (void*)0)");
            const auto label = nextLabel++;
            const auto temporary = [&] { Put("ls_environment_", label); };
            Put("({ LS_LambdaEnv_", index, "* ", temporary, "=(LS_LambdaEnv_", index, "*)ls_alloc(sizeof(LS_LambdaEnv_", index, "),ls_trace_lambda_env_", index, "); ");
            for (const auto* capture : lambdaCaptures[index]) {
                Put(temporary, "->", Id{capture->name}, '=');
                if (capture->name == "this") Put("self"); else Put(Id{capture->name});
                Put("; ");
            }
            return Put("ls_closure_create(ls_lambda_", index, ", ", temporary, "); })");
        }
        case SyntaxExpression::Kind::TableEntry:
        case SyntaxExpression::Kind::Slice: case SyntaxExpression::Kind::Range:
            throw std::runtime_error("Native C backend does not support this expression kind yet (type '" + Name(value.type) + "')");
        }
        throw std::runtime_error("Native C backend encountered an unknown expression");
    }

    void Call(const BoundExpression& value, std::string_view owner) {
        assert(value.kind == SyntaxExpression::Kind::Call && value.children.count >= 1);
        const auto& callee = Child(value, 0);
        const size_t childCount = value.children.count;
        const auto argument = [&](size_t index) { return Emitted(Child(value, index + 1), owner); };
        if (value.lowering == Lowering::Printf) {
            const size_t count = childCount - 2;
            if (!count) return Put("ls_printf(", argument(0), ", 0, (const LS_Value*)0)");
            const auto label = nextLabel++;
            const auto arguments = [&] { Put("ls_printf_arguments_", label); };
            Put("({ LS_Value ", arguments, '[', count, "]; ");
            for (size_t i{}; i < count; ++i) Put(arguments, '[', i, "]=", Boxed(Child(value, i + 2), owner), "; ");
            return Put("ls_printf(", argument(0), ", ", count, ", ", arguments, "); })");
        }
        if (types.IsFunction(callee.type)) {
            const bool hasArguments = childCount > 1;
            const auto label = hasArguments ? nextLabel++ : 0;
            const auto arguments = [&] { if (hasArguments) Put("ls_arguments_", label); else Put("(const LS_Value*)0"); };
            Put("({ ");
            if (hasArguments) {
                Put("LS_Value ", arguments, '[', childCount - 1, "]; ");
                for (size_t i = 1; i < childCount; ++i) Put(arguments, '[', i - 1, "]=", Boxed(Child(value, i), owner), "; ");
            }
            const auto function = Emitted(callee, owner);
            if (value.type == TypeTable::Void) return Put("ls_closure_call(", function, ", ", arguments, ", (LS_Value*)0); })");
            if (value.type == TypeTable::Any) return Put("LS_Value ls_result = {0}; ls_closure_call(", function, ", ", arguments, ", &ls_result); ls_result; })");
            const auto access = value.type == TypeTable::Int || IsEnum(value.type) ? "as.integer" : value.type == TypeTable::Float ? "as.number" : value.type == TypeTable::Bool ? "as.boolean" : "as.pointer";
            return Put("LS_Value ls_result = {0}; ls_closure_call(", function, ", ", arguments, ", &ls_result); ls_result.", access, "; })");
        }
        const auto receiver = [&] { Expression(Child(callee, 0), owner); };
        switch (value.lowering) {
        case Lowering::CastIdentity: case Lowering::CastIntFloat: case Lowering::CastNumberBool: return Put('(', Type(value.type), ")(", argument(0), ')');
        case Lowering::CastBoolNumber: return Put('(', argument(0), " != 0)");
        case Lowering::CastBoolString: return Put("ls_string_truthy(", argument(0), ')');
        case Lowering::CastNumberString: throw std::runtime_error("Native C backend does not support parsing numeric strings yet");
        case Lowering::StringLength: return Put("ls_string_length(", receiver, ')');
        case Lowering::StringContains: return Put("ls_string_contains(", receiver, ", ", argument(0), ')');
        case Lowering::StringStartsWith: return Put("ls_string_starts_with(", receiver, ", ", argument(0), ')');
        case Lowering::StringEndsWith: return Put("ls_string_ends_with(", receiver, ", ", argument(0), ')');
        case Lowering::CastString: case Lowering::StringSubstring: case Lowering::StringLower: case Lowering::StringUpper:
            throw std::runtime_error("Native C backend does not support allocating string operation '" + std::string(LoweringName(value.lowering)) + "' yet");
        default: break;
        }
        bool hasReceiver = true;
        if (callee.lowering == Lowering::ImplicitMethod) Put(Fn{owner, callee.Text()}, "(self");
        else if (callee.kind == SyntaxExpression::Kind::Member && value.lowering != Lowering::StaticCall) {
            const auto& target = Child(callee, 0);
            Put(Fn{Name(target.type), callee.Text()}, '(', Emitted(target, owner));
        } else { Put(Emitted(callee, owner), '('); hasReceiver = false; }
        for (size_t i = 1; i < childCount; ++i) {
            if (i != 1 || hasReceiver) Put(", ");
            Expression(Child(value, i), owner);
        }
        Put(')');
    }

    void Statements(NodeRange statements, size_t depth, std::string_view owner = {}, std::optional<size_t> continueLabel = {}, bool moduleTopLevel = false) {
        for (const auto& statement : program.Statements(statements)) {
            const auto& expression = program.Expression(statement.expression);
            switch (statement.kind) {
            case SyntaxStatement::Kind::Local:
            {
                const auto target = [&] { if (moduleTopLevel) Put("ls_state."); Put(Id{statement.Name()}); };
                const auto declaration = [&] { if (!moduleTopLevel) Put(Type(statement.type), ' '); };
                if (statement.type == TypeTable::Any && expression.kind == SyntaxExpression::Kind::Index && Child(expression, 0).type == TypeTable::Table) {
                    Line(depth, declaration, target, " = (LS_Value){0};");
                    Line(depth, "ls_table_get(", Emitted(Child(expression, 0), owner), ", &(", Boxed(Child(expression, 1), owner), "), &", target, ");");
                } else if (expression.kind == SyntaxExpression::Kind::Missing) Line(depth, declaration, target, " = ", [&] { Default(Name(statement.type)); }, ';');
                else Line(depth, declaration, target, " = ", Emitted(expression, owner), ';');
                break;
            }
            case SyntaxStatement::Kind::Expression: Line(depth, Emitted(expression, owner), ';'); break;
            case SyntaxStatement::Kind::Return:
                if (expression.kind == SyntaxExpression::Kind::Missing) Line(depth, "return;");
                else Line(depth, "return ", Emitted(expression, owner), ';');
                break;
            case SyntaxStatement::Kind::Break: Line(depth, "break;"); break;
            case SyntaxStatement::Kind::Continue:
                if (continueLabel) Line(depth, "goto ls_continue_", *continueLabel, ';');
                else Line(depth, "continue;");
                break;
            case SyntaxStatement::Kind::If:
                Line(depth, "if (", Emitted(expression, owner), ") {"); Statements(statement.body, depth + 1, owner, continueLabel); Line(depth, '}');
                if (statement.elseBody.count) { Line(depth, "else {"); Statements(statement.elseBody, depth + 1, owner, continueLabel); Line(depth, '}'); }
                break;
            case SyntaxStatement::Kind::While:
                Line(depth, "while (", Emitted(expression, owner), ") {"); Statements(statement.body, depth + 1, owner); Line(depth, '}'); break;
            case SyntaxStatement::Kind::For:
            {
                const auto label = nextLabel++;
                const auto& increment = program.Expression(statement.increment);
                Line(depth, '{'); Statements(statement.initializer, depth + 1, owner);
                Line(depth + 1, "while (", Emitted(expression, owner), ") {");
                Statements(statement.body, depth + 2, owner, label);
                Line(depth + 2, "ls_continue_", label, ":;");
                if (increment.kind != SyntaxExpression::Kind::Missing) Line(depth + 2, Emitted(increment, owner), ';');
                Line(depth + 1, '}'); Line(depth, '}'); break;
            }
            case SyntaxStatement::Kind::ForEach:
                throw std::runtime_error("Native C backend does not support foreach/range loops yet");
            }
        }
    }

    void Parameters(const BoundFunction& function) {
        if (function.parameters.empty()) return Put("void");
        for (size_t i{}; i < function.parameters.size(); ++i) {
            if (i) Put(", ");
            Put(CTypeName{function.parameters[i].type}, ' ', Id{function.parameters[i].name});
        }
    }
    static const BoundFunction* Constructor(const BoundClass& type) {
        const auto found = std::find_if(type.functions.begin(), type.functions.end(), [](const auto& item) { return item.isConstructor; });
        return found == type.functions.end() ? nullptr : &*found;
    }
    // "LS_T* ls_T_new(...)"
    void ConstructorSignature(const BoundClass& type) {
        Put("LS_", Id{type.name}, "* ", Fn{type.name, "new"}, '(');
        if (const auto* constructor = Constructor(type)) Parameters(*constructor); else Put("void");
        Put(')');
    }
    // "R ls_T_name(LS_T* self, ...)"
    void MethodSignature(const BoundClass& type, const BoundFunction& function) {
        Put(CTypeName{function.returnType}, ' ', Fn{type.name, function.name}, "(LS_", Id{type.name}, "* self");
        for (const auto& parameter : function.parameters) Put(", ", CTypeName{parameter.type}, ' ', Id{parameter.name});
        Put(')');
    }

    CCompilationResult Run() {
        Line(0, "/* generated by LuaSharp; C11 */");
        Line(0, "typedef signed long long ls_int; /* public LuaSharp int: signed 64-bit */"); Line(0, "typedef unsigned long long ls_wrap_bits; /* internal arithmetic bits; never a LuaSharp value */"); Line(0, "typedef double ls_float;"); Line(0, "typedef unsigned char ls_bool;");
        Line(0, "typedef struct LS_String { ls_int length; char data[1]; } LS_String;");
        Line(0, "typedef LS_String* ls_string;");
        Line(0, "typedef void LS_Table; typedef LS_Table* ls_table;");
        Line(0, "typedef struct LS_Value { ls_wrap_bits kind; union { ls_int integer; ls_float number; ls_wrap_bits boolean; void* pointer; } as; } LS_Value;");
        Line(0, "typedef struct LS_Closure { void (*invoke)(void* environment, const LS_Value* arguments, LS_Value* result); void* environment; } LS_Closure;");
        Line(0, "typedef void (*LS_TraceFunction)(void* runtime_context, void* object);");
        Line(0, "typedef struct LS_RuntimeApi { ls_wrap_bits abi_version; void* context; void* (*allocate)(void* context, ls_wrap_bits size, LS_TraceFunction trace); void (*mark)(void* context, void* object); void* (*intern)(void* context, const char* value); void* (*table_create)(void* context, ls_wrap_bits count, const LS_Value* pairs); void (*table_get)(void* context, void* table, const LS_Value* key, LS_Value* out); void (*table_set)(void* context, void* table, const LS_Value* key, const LS_Value* value); void (*print)(void* context, void* format, ls_wrap_bits count, const LS_Value* arguments); void (*report_error)(void* context, const char* message); } LS_RuntimeApi;");
        Line(0, "static LS_RuntimeApi* ls_runtime;");
        Line(0, "static void ls_report_error(const char* message) { if (ls_runtime && ls_runtime->report_error) ls_runtime->report_error(ls_runtime->context, message); }");
        Line(0, "static void* ls_alloc(ls_wrap_bits size, LS_TraceFunction trace) { void* value; if (!ls_runtime || !ls_runtime->allocate) return (void*)0; value = ls_runtime->allocate(ls_runtime->context, size, trace); if (!value) ls_report_error(\"LuaSharp allocation failed\"); return value; }");
        Line(0, "static ls_string ls_intern(const char* value) { return ls_runtime && ls_runtime->intern ? (ls_string)ls_runtime->intern(ls_runtime->context, value) : (ls_string)0; }");
        Line(0, "static void ls_mark_value(void* context, const LS_Value* value) { if (value && value->kind >= 4 && value->kind <= 7) ls_runtime->mark(context, value->as.pointer); }");
        Line(0, "static void ls_trace_closure(void* context, void* object) { LS_Closure* closure=(LS_Closure*)object; if(closure->environment) ls_runtime->mark(context, closure->environment); }");
        Line(0, "static LS_Closure* ls_closure_create(void (*invoke)(void*, const LS_Value*, LS_Value*), void* environment) { LS_Closure* closure=(LS_Closure*)ls_alloc(sizeof(LS_Closure), ls_trace_closure); if(!closure)return (LS_Closure*)0; closure->invoke=invoke; closure->environment=environment; return closure; }");
        Line(0, "static void ls_closure_call(LS_Closure* closure, const LS_Value* arguments, LS_Value* result) { if(!closure||!closure->invoke){ls_report_error(\"attempt to call null closure\");return;} closure->invoke(closure->environment,arguments,result); }");
        Line(0, "static ls_table ls_table_create(ls_wrap_bits count, const LS_Value* pairs) { return ls_runtime && ls_runtime->table_create ? (ls_table)ls_runtime->table_create(ls_runtime->context, count, pairs) : (ls_table)0; }");
        Line(0, "static void ls_table_get(ls_table table, const LS_Value* key, LS_Value* out) { if (ls_runtime && ls_runtime->table_get) ls_runtime->table_get(ls_runtime->context, table, key, out); }");
        Line(0, "static void ls_table_set(ls_table table, const LS_Value* key, const LS_Value* value) { if (ls_runtime && ls_runtime->table_set) ls_runtime->table_set(ls_runtime->context, table, key, value); }");
        Line(0, "static void ls_printf(ls_string format, ls_wrap_bits count, const LS_Value* arguments) { if (ls_runtime && ls_runtime->print) ls_runtime->print(ls_runtime->context, format, count, arguments); }");
        Line(0, "static ls_int ls_int_add(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a + (ls_wrap_bits)b); }");
        Line(0, "static ls_int ls_int_sub(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a - (ls_wrap_bits)b); }");
        Line(0, "static ls_int ls_int_mul(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a * (ls_wrap_bits)b); }");
        Line(0, "static ls_int ls_int_neg(ls_int a) { return (ls_int)(0ULL - (ls_wrap_bits)a); }");
        Line(0, "static ls_float ls_int_div(ls_int a, ls_int b) { return (ls_float)a / (ls_float)b; }");
        Line(0, "static ls_int ls_int_mod(ls_int a, ls_int b) { ls_int r; if (!b) { ls_report_error(\"integer remainder by zero\"); return 0; } if (b == -1) return 0; r = a % b; if (r && ((r < 0) != (b < 0))) r = ls_int_add(r, b); return r; }");
        Line(0, "static ls_bool ls_value_equal(const LS_Value* a, const LS_Value* b) { if(a->kind==1&&b->kind==2)return (ls_float)a->as.integer==b->as.number; if(a->kind==2&&b->kind==1)return a->as.number==(ls_float)b->as.integer; if(a->kind!=b->kind)return 0; if(a->kind==0)return 1; if(a->kind==1)return a->as.integer==b->as.integer; if(a->kind==2)return a->as.number==b->as.number; if(a->kind==3)return a->as.boolean==b->as.boolean; return a->as.pointer==b->as.pointer; }");
        Line(0, "static ls_bool ls_value_compare(const LS_Value* a, int operation, const LS_Value* b) { ls_float left,right; if((a->kind!=1&&a->kind!=2)||(b->kind!=1&&b->kind!=2)){ls_report_error(\"dynamic comparison requires numbers\");return 0;} left=a->kind==1?(ls_float)a->as.integer:a->as.number; right=b->kind==1?(ls_float)b->as.integer:b->as.number; return operation==0?left<right:operation==1?left<=right:operation==2?left>right:left>=right; }");
        Line(0, "static void ls_value_arithmetic(const LS_Value* a, int operation, const LS_Value* b, LS_Value* out) { if((a->kind!=1&&a->kind!=2)||(b->kind!=1&&b->kind!=2)){ls_report_error(\"dynamic arithmetic requires numbers\");*out=(LS_Value){0};return;} if(operation==4){if(a->kind!=1||b->kind!=1){ls_report_error(\"dynamic remainder requires integers\");*out=(LS_Value){0};return;} out->kind=1;out->as.integer=ls_int_mod(a->as.integer,b->as.integer);return;} if(operation==3||a->kind==2||b->kind==2){ls_float left=a->kind==1?(ls_float)a->as.integer:a->as.number,right=b->kind==1?(ls_float)b->as.integer:b->as.number;out->kind=2;out->as.number=operation==0?left+right:operation==1?left-right:operation==2?left*right:left/right;return;} out->kind=1;out->as.integer=operation==0?ls_int_add(a->as.integer,b->as.integer):operation==1?ls_int_sub(a->as.integer,b->as.integer):ls_int_mul(a->as.integer,b->as.integer); }");
        Line(0, "static ls_int ls_string_length(ls_string s) { return s ? s->length : 0; }");
        Line(0, "static ls_bool ls_string_starts_with(ls_string s, ls_string p) { ls_int i; if (!s || !p || p->length > s->length) return 0; for (i=0;i<p->length;++i) if (s->data[i]!=p->data[i]) return 0; return 1; }");
        Line(0, "static ls_bool ls_string_ends_with(ls_string s, ls_string p) { ls_int i,offset; if (!s || !p || p->length > s->length) return 0; offset=s->length-p->length; for(i=0;i<p->length;++i) if(s->data[offset+i]!=p->data[i]) return 0; return 1; }");
        Line(0, "static ls_bool ls_string_contains(ls_string s, ls_string p) { ls_int i,j; if(!s||!p)return 0; if(!p->length)return 1; for(i=0;i+p->length<=s->length;++i){for(j=0;j<p->length&&s->data[i+j]==p->data[j];++j){} if(j==p->length)return 1;} return 0; }");
        Line(0, "static ls_bool ls_string_equal(ls_string a, ls_string b) { return a==b; }");
        Line(0, "static ls_bool ls_string_truthy(ls_string s) { return s && s->length != 0; }");
        Line(0);
        for (const auto& value : program.classes) Line(0, "typedef struct LS_", Id{value.name}, " LS_", Id{value.name}, ';');
        for (const auto& value : externalTypes) if (!classes.contains(value.name) && !value.isEnum) Line(0, "typedef struct LS_", Id{value.name}, " LS_", Id{value.name}, ';');
        if (!program.classes.empty() || !externalTypes.empty()) Line(0);
        for (const auto& value : externalTypes) if (!value.isEnum) for (const auto& member : value.members) if (member.kind == SymbolKind::Method) {
            const auto parameters = [&] {
                Put("LS_", Id{value.name}, "* self");
                for (const auto& parameter : member.parameters) {
                    const auto split = parameter.find(' ');
                    auto parameterType = std::string_view(parameter).substr(0, split);
                    if (parameterType.ends_with('?')) parameterType.remove_suffix(1);
                    const auto parameterName = split == std::string::npos ? std::string_view("argument") : std::string_view(parameter).substr(split + 1);
                    Put(", ", CTypeName{parameterType}, ' ', Id{parameterName});
                }
            };
            Line(0, "extern ", CTypeName{member.type}, ' ', Fn{value.name, member.name}, '(', parameters, ");");
        }
        if (!externalTypes.empty()) Line(0);
        for (const auto& value : program.enums) {
            for (const auto& member : value.members) Line(0, "#define LS_", Id{value.name}, '_', Id{member.name}, " ((ls_int)", member.value, ')');
        }
        if (!program.enums.empty()) Line(0);
        for (const auto& value : program.classes) {
            if (!value.baseName.empty()) throw std::runtime_error("Native C backend does not support class inheritance yet ('" + value.name + "')");
            Line(0, "struct LS_", Id{value.name}, " {");
            for (const auto& field : value.fields) Line(1, CTypeName{field.type}, ' ', Id{field.name}, ';');
            if (value.fields.empty()) Line(1, "unsigned char _unused;");
            Line(0, "};");
        }
        for (size_t i{}; i < lambdas.size(); ++i) if (!lambdaCaptures[i].empty()) {
            Line(0, "typedef struct LS_LambdaEnv_", i, " {");
            for (const auto* capture : lambdaCaptures[i]) Line(1, Type(capture->type), ' ', Id{capture->name}, ';');
            Line(0, "} LS_LambdaEnv_", i, ';');
            Line(0, "static void ls_trace_lambda_env_", i, "(void* runtime_context, void* object) {");
            Line(1, "LS_LambdaEnv_", i, "* ls_env=(LS_LambdaEnv_", i, "*)object;");
            bool traced = false;
            for (const auto* capture : lambdaCaptures[i]) if (IsManagedObject(Name(capture->type))) {
                traced = true;
                if (capture->type == TypeTable::Any) Line(1, "ls_mark_value(runtime_context, &ls_env->", Id{capture->name}, ");");
                else Line(1, "ls_runtime->mark(runtime_context, ls_env->", Id{capture->name}, ");");
            }
            if (!traced) Line(1, "(void)runtime_context; (void)ls_env;");
            Line(0, '}');
        }
        if (!program.classes.empty()) Line(0);
        for (const auto& value : program.classes) {
            Line(0, "static void ls_trace_", Id{value.name}, "(void* runtime_context, void* object) {");
            Line(1, "LS_", Id{value.name}, "* self = (LS_", Id{value.name}, "*)object;");
            bool tracesField = false;
            for (const auto& field : value.fields) if (IsManagedObject(field.type)) {
                tracesField = true;
                if (field.type == "any") Line(1, "ls_mark_value(runtime_context, &self->", Id{field.name}, ");");
                else Line(1, "ls_runtime->mark(runtime_context, self->", Id{field.name}, ");");
            }
            if (!tracesField) Line(1, "(void)runtime_context; (void)self;");
            Line(0, '}');
        }
        Line(0, "typedef struct LS_ModuleState {");
        bool hasGlobals = false;
        for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local) {
            hasGlobals = true; Line(1, Type(statement.type), ' ', Id{statement.Name()}, ';');
        }
        if (!hasGlobals) Line(1, "unsigned char _unused;");
        Line(0, "} LS_ModuleState;");
        Line(0, "static LS_ModuleState ls_state;");
        Line(0, "void ls_module_trace_roots(void* runtime_context) {");
        bool tracesGlobal = false;
        for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local && IsManagedObject(Name(statement.type))) {
            tracesGlobal = true;
            if (statement.type == TypeTable::Any) Line(1, "ls_mark_value(runtime_context, &ls_state.", Id{statement.Name()}, ");");
            else Line(1, "ls_runtime->mark(runtime_context, ls_state.", Id{statement.Name()}, ");");
        }
        if (!tracesGlobal) Line(1, "(void)runtime_context;");
        Line(0, '}');
        Line(0);
        for (size_t i{}; i < lambdas.size(); ++i) Line(0, "static void ls_lambda_", i, "(void* environment, const LS_Value* arguments, LS_Value* result);");
        if (!lambdas.empty()) Line(0);
        CCompilationResult result;
        for (const auto& value : program.classes) {
            NativeTypeLayout layout; layout.name = value.name; layout.baseName = value.baseName;
            for (const auto& field : value.fields) layout.fields.push_back({field.name, field.type});
            result.types.push_back(std::move(layout));
        }
        Line(0, "ls_int ls_module_abi_version(void) { return 4; }");
        Line(0, "int ls_module_bind(LS_RuntimeApi* runtime) { if (!runtime || runtime->abi_version != 4 || !runtime->allocate || !runtime->mark || !runtime->intern || !runtime->table_create || !runtime->table_get || !runtime->table_set || !runtime->print || !runtime->report_error) return -1; ls_runtime = runtime; return 0; }");
        result.exports.push_back({"$abi_version", "ls_module_abi_version", "int", {}});
        result.exports.push_back({"$module_bind", "ls_module_bind", "int", {"$runtime_api"}});
        for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local) {
            const auto symbol = "ls_global_" + Identifier(statement.Name());
            Line(0, "void* ", symbol, "(void) { return &ls_state.", Id{statement.Name()}, "; }");
            result.globals.push_back({statement.Name(), symbol, Name(statement.type)});
        }
        for (const auto& value : program.classes) {
            Line(0, [&] { ConstructorSignature(value); }, ';');
            for (const auto& function : value.functions) if (!function.isConstructor) Line(0, [&] { MethodSignature(value, function); }, ';');
        }
        Line(0);
        for (const auto& value : program.classes) {
            const auto* constructor = Constructor(value);
            Line(0, [&] { ConstructorSignature(value); }, " {");
            Line(1, "LS_", Id{value.name}, "* self = (LS_", Id{value.name}, "*)ls_alloc(sizeof(LS_", Id{value.name}, "), ls_trace_", Id{value.name}, ");");
            Line(1, "if (!self) return (LS_", Id{value.name}, "*)0;");
            for (const auto& field : value.fields) {
                const auto& initializer = program.Expression(field.initializer);
                Line(1, "self->", Id{field.name}, " = ", [&] { if (initializer.kind == SyntaxExpression::Kind::Missing) Default(field.type); else Expression(initializer, value.name); }, ';');
            }
            if (constructor) Statements(constructor->body, 1, value.name);
            Line(1, "return self;"); Line(0, '}');
            std::vector<std::string> constructorParameters;
            if (constructor) for (const auto& parameter : constructor->parameters) constructorParameters.push_back(parameter.type);
            result.exports.push_back({value.name + ".new", FunctionName(value.name, "new"), value.name, std::move(constructorParameters)});
            for (const auto& function : value.functions) {
                if (function.isConstructor) continue;
                Line(0, [&] { MethodSignature(value, function); }, " {");
                Statements(function.body, 1, value.name); Line(0, '}');
                std::vector<std::string> parameters{value.name}; for (const auto& p : function.parameters) parameters.push_back(p.type);
                result.exports.push_back({value.name + "." + function.name, FunctionName(value.name, function.name), function.returnType, std::move(parameters)});
            }
        }
        for (size_t i{}; i < lambdas.size(); ++i) {
            const auto& lambda = *lambdas[i];
            Line(0, "static void ls_lambda_", i, "(void* environment, const LS_Value* arguments, LS_Value* result) {");
            if (lambdaCaptures[i].empty()) Line(1, "(void)environment;");
            else {
                Line(1, "LS_LambdaEnv_", i, "* ls_env=(LS_LambdaEnv_", i, "*)environment;");
                const auto self = std::find_if(lambdaCaptures[i].begin(), lambdaCaptures[i].end(), [](const auto* value) { return value->name == "this"; });
                if (self != lambdaCaptures[i].end()) Line(1, Type((*self)->type), " self=ls_env->", Id{(*self)->name}, ';');
            }
            Line(1, "(void)arguments;");
            const auto parameters = analysis.syntax.Parameters(*lambda.syntax);
            for (size_t parameter{}; parameter < parameters.size(); ++parameter)
                Line(1, "LS_Value ", Id{parameters[parameter].name}, "=arguments[", parameter, "];");
            const auto returnType = types.Element(lambda.type);
            activeLambda = i;
            if (lambda.LambdaIsBlock()) {
                if (returnType != TypeTable::Void) throw std::runtime_error("Native block closures returning values are not supported yet");
                Line(1, "(void)result;");
                Statements(lambda.lambdaBody, 1);
            } else if (returnType == TypeTable::Void) {
                Line(1, "(void)result;");
                Line(1, Emitted(Child(lambda, 0), {}), ';');
            } else Line(1, "if(result) *result = ", Boxed(Child(lambda, 0), {}), ';');
            activeLambda.reset();
            Line(0, '}');
        }
        if (!lambdas.empty()) Line(0);
        Line(0, "int ls_module_init(void) {"); Statements(program.topLevel, 1, {}, {}, true); Line(1, "return 0;"); Line(0, '}');
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
