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
    if (type.ends_with("[]")) { out += "LS_List*"; return; }
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
    TypeId functionReturn = TypeTable::Unknown; // return type of the method being emitted
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
    // Unique statement-expression temporary "ls_<name>_<label>".
    auto Temp(std::string_view name, size_t label) { return [this, name, label] { Put("ls_", name, '_', label); }; }

    const std::string& Name(TypeId type) const { return types.Name(type); }
    CTypeName Type(TypeId type) const { return {Name(type)}; }
    TypeId Find(std::string_view type) const { return types.Find(type).value_or(TypeTable::Unknown); }
    bool IsEnum(TypeId type) const { return enums.contains(Name(type)); }
    bool IsManagedObject(std::string_view type) const { return type == "string" || type == "table" || type == "any" || type.starts_with("$function:") || type.ends_with("[]") || classes.contains(type); }
    // NativeElementKind: 0 plain, 1 managed pointer, 2 LS_Value.
    int ElementKind(std::string_view element) const { return element == "any" ? 2 : IsManagedObject(element) ? 1 : 0; }
    void NewList(std::string_view element, size_t capacity) { Put("ls_list_create(sizeof(", CTypeName{element}, "), ", ElementKind(element), ", ", capacity, ')'); }
    const BoundExpression& Child(const BoundExpression& value, size_t index) const { return program.Child(value, index); }
    const BoundExpression& ListIndex(const BoundExpression& index) const {
        if (index.type != TypeTable::Int) throw std::runtime_error("Native list indices must have type 'int', got '" + Name(index.type) + "'");
        return index;
    }
    // Lists and tables are separate native objects, although the language lets either flow into the other.
    void CheckFlow(TypeId target, TypeId source) const {
        if ((types.IsList(target) && source == TypeTable::Table) || (target == TypeTable::Table && types.IsList(source)))
            throw std::runtime_error("Native C backend cannot convert between 'table' and '" + Name(types.IsList(target) ? target : source) + "'");
    }
    const BoundFunction* FindFunction(std::string_view type, std::string_view name, bool constructor = false) const {
        const auto found = classes.find(type);
        if (found == classes.end()) return nullptr;
        const auto& functions = found->second->functions;
        const auto function = std::find_if(functions.begin(), functions.end(), [&](const auto& item) { return constructor ? item.isConstructor : !item.isConstructor && item.name == name; });
        return function == functions.end() ? nullptr : &*function;
    }
    // Arguments start at child `first` of value.
    void CheckArguments(const BoundFunction* function, const BoundExpression& value, size_t first) const {
        if (!function) return;
        for (size_t i{}; i < function->parameters.size() && first + i < value.children.count; ++i)
            CheckFlow(Find(function->parameters[i].type), Child(value, first + i).type);
    }
    // lhs <op>= rhs for one-character op.
    void Combine(TypeId type, char operation, const auto& left, const auto& right) {
        if (type == TypeTable::String && operation == '+') return Put("ls_string_concat(", left, ", ", right, ')');
        if (type == TypeTable::Int) {
            const auto function = operation == '+' ? "ls_int_add" : operation == '-' ? "ls_int_sub" : operation == '*' ? "ls_int_mul" : operation == '%' ? "ls_int_mod" : nullptr;
            if (function) return Put(function, '(', left, ", ", right, ')');
        }
        Put('(', left, ' ', operation, ' ', right, ')');
    }
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
        if (type.ends_with("[]")) return NewList(type.substr(0, type.size() - 2), 0);
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
        if (types.IsList(type)) return boxed("((LS_Value){.kind=8,.as.pointer=(");
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
            CheckArguments(FindFunction(text, {}, true), value, 0);
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
            if (value.lowering == Lowering::Concat) return Put("ls_string_concat(", child(0), ", ", child(1), ')');
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
            const bool compound = text != "=";
            assert(!compound || text.size() == 2);
            if (!compound) CheckFlow(target.type, Child(value, 1).type);
            const auto label = nextLabel++;
            const auto slot = Temp("slot", label), update = Temp("value", label);
            const auto store = [&] { if (compound) Combine(target.type, text.front(), [&] { Put('*', slot); }, update); else Put(update); };
            if (value.lowering == Lowering::ListSet) {
                const auto list = Temp("list", label), index = Temp("index", label);
                // Evaluates list, index, then value; the slot address is taken last because the value may grow the list.
                return Put("({ LS_List* ", list, " = ", Emitted(Child(target, 0), owner), "; ls_int ", index, " = ", Emitted(ListIndex(Child(target, 1)), owner), "; ",
                    Type(target.type), ' ', update, " = ", child(1), "; ", Type(target.type), "* ", slot, " = (", Type(target.type), "*)ls_list_at(", list, ", ", index, "); *", slot, " = ", store, "; })");
            }
            if (!compound) return Put('(', child(0), " = ", child(1), ')');
            if (target.kind == SyntaxExpression::Kind::Identifier) return Put('(', child(0), " = ", [&] { Combine(target.type, text.front(), child(0), child(1)); }, ')');
            return Put("({ ", Type(target.type), "* ", slot, " = &(", child(0), "); ", Type(target.type), ' ', update, " = ", child(1), "; *", slot, " = ", store, "; })");
        }
        case SyntaxExpression::Kind::Table: {
            if (types.IsList(value.type)) {
                const auto& element = Name(types.Element(value.type));
                if (!childCount) return NewList(element, 0);
                const auto literal = Temp("literal", nextLabel++);
                Put("({ LS_List* ", literal, " = "); NewList(element, childCount); Put("; ");
                for (size_t i{}; i < childCount; ++i) Put("*(", CTypeName{element}, "*)ls_list_push(", literal, ") = ", child(i), "; ");
                return Put(literal, "; })");
            }
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
            if (value.lowering == Lowering::ListIndex) return Put("(*(", Type(value.type), "*)ls_list_at(", child(0), ", ", Emitted(ListIndex(Child(value, 1)), owner), "))");
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
        case SyntaxExpression::Kind::Slice: return Put("ls_list_slice(", child(0), ", ", child(1), ", ", child(2), ')');
        case SyntaxExpression::Kind::TableEntry: case SyntaxExpression::Kind::Range:
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
        case Lowering::CastNumberString: return Put(value.type == TypeTable::Int ? "ls_string_to_int(" : "ls_string_to_float(", argument(0), ')');
        case Lowering::CastString: {
            const auto boxed = Temp("boxed", nextLabel++);
            return Put("({ LS_Value ", boxed, " = ", Boxed(Child(value, 1), owner), "; ls_to_string(&", boxed, "); })");
        }
        case Lowering::StringLength: return Put("ls_string_length(", receiver, ')');
        case Lowering::StringContains: return Put("ls_string_contains(", receiver, ", ", argument(0), ')');
        case Lowering::StringStartsWith: return Put("ls_string_starts_with(", receiver, ", ", argument(0), ')');
        case Lowering::StringEndsWith: return Put("ls_string_ends_with(", receiver, ", ", argument(0), ')');
        case Lowering::StringSubstring:
            if (childCount == 3) return Put("ls_string_substring(", receiver, ", ", argument(0), ", ", argument(1), ", 1)");
            return Put("ls_string_substring(", receiver, ", ", argument(0), ", 0, 0)");
        case Lowering::StringLower: return Put("ls_string_case(", receiver, ", 0)");
        case Lowering::StringUpper: return Put("ls_string_case(", receiver, ", 1)");
        case Lowering::ListPushBack: {
            const auto element = types.Element(Child(callee, 0).type);
            CheckFlow(element, Child(value, 1).type);
            const auto label = nextLabel++;
            const auto list = Temp("list", label), pushed = Temp("value", label);
            return Put("({ LS_List* ", list, " = ", receiver, "; ", Type(element), ' ', pushed, " = ", argument(0), "; *(", Type(element), "*)ls_list_push(", list, ") = ", pushed, "; })");
        }
        case Lowering::ListPopBack: return Put("(*(", Type(value.type), "*)ls_list_pop(", receiver, "))");
        case Lowering::ListSize: return Put("ls_list_size(", receiver, ')');
        case Lowering::ListEmpty: return Put("(ls_list_size(", receiver, ") == 0)");
        case Lowering::ListClear: return Put("ls_list_clear(", receiver, ')');
        default: break;
        }
        bool hasReceiver = true;
        if (callee.lowering == Lowering::ImplicitMethod) {
            CheckArguments(FindFunction(owner, callee.Text()), value, 1);
            Put(Fn{owner, callee.Text()}, "(self");
        } else if (callee.kind == SyntaxExpression::Kind::Member && value.lowering != Lowering::StaticCall) {
            const auto& target = Child(callee, 0);
            CheckArguments(FindFunction(Name(target.type), callee.Text()), value, 1);
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
                CheckFlow(statement.type, expression.type);
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
                else { CheckFlow(functionReturn, expression.type); Line(depth, "return ", Emitted(expression, owner), ';'); }
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
            case SyntaxStatement::Kind::ForEach: ForEach(statement, depth, owner); break;
            }
        }
    }

    void ForEach(const BoundStatement& statement, size_t depth, std::string_view owner) {
        const auto& iterable = program.Expression(statement.expression);
        const auto label = nextLabel++;
        const auto name = Id{statement.Name()}, secondName = Id{statement.SecondName()};
        const bool hasSecond = !statement.SecondName().empty();
        Line(depth, '{');
        if (iterable.kind == SyntaxExpression::Kind::Range) {
            // Inclusive bounds, evaluated once; the counter is separate so the body cannot change the iteration.
            const auto counter = Temp("counter", label), last = Temp("last", label);
            Line(depth + 1, "ls_int ", counter, " = ", Emitted(Child(iterable, 0), owner), ", ", last, " = ", Emitted(Child(iterable, 1), owner), ';');
            Line(depth + 1, "if (", counter, " <= ", last, ") for (;;) {");
            Line(depth + 2, "ls_int ", name, " = ", counter, ';');
            Statements(statement.body, depth + 2, owner, label);
            Line(depth + 2, "ls_continue_", label, ":;");
            Line(depth + 2, "if (", counter, " == ", last, ") break;");
            Line(depth + 2, "++", counter, ';');
        } else if (types.IsList(iterable.type)) {
            // Reads the count on every pass, like ipairs.
            const auto element = types.Element(iterable.type);
            const auto list = Temp("list", label), index = Temp("index", label);
            Line(depth + 1, "LS_List* ", list, " = ", Emitted(iterable, owner), "; ls_int ", index, ';');
            Line(depth + 1, "for (", index, " = 0; ", index, " < ls_list_size(", list, "); ++", index, ") {");
            if (hasSecond) Line(depth + 2, "ls_int ", name, " = ", index, ';');
            Line(depth + 2, Type(element), ' ', hasSecond ? secondName : name, " = *(", Type(element), "*)(", list, "->data + ", index, " * (ls_int)", list, "->element_size);");
            Statements(statement.body, depth + 2, owner);
        } else if (iterable.type == TypeTable::Table) {
            assert(hasSecond);
            const auto table = Temp("table", label), cursor = Temp("cursor", label), key = Temp("key", label), item = Temp("item", label);
            Line(depth + 1, "ls_table ", table, " = ", Emitted(iterable, owner), "; ls_wrap_bits ", cursor, " = 0; LS_Value ", key, ", ", item, ';');
            Line(depth + 1, "while (ls_table_next(", table, ", &", cursor, ", &", key, ", &", item, ")) {");
            Line(depth + 2, "LS_Value ", name, " = ", key, "; LS_Value ", secondName, " = ", item, ';');
            Statements(statement.body, depth + 2, owner);
        } else throw std::runtime_error("Native C backend cannot iterate a value of type '" + Name(iterable.type) + "'");
        Line(depth + 1, '}');
        Line(depth, '}');
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
        Line(0, "#include <luasharp/runtime.h>");
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
                CheckFlow(Find(field.type), initializer.type);
                Line(1, "self->", Id{field.name}, " = ", [&] { if (initializer.kind == SyntaxExpression::Kind::Missing) Default(field.type); else Expression(initializer, value.name); }, ';');
            }
            functionReturn = TypeTable::Void;
            if (constructor) Statements(constructor->body, 1, value.name);
            Line(1, "return self;"); Line(0, '}');
            std::vector<std::string> constructorParameters;
            if (constructor) for (const auto& parameter : constructor->parameters) constructorParameters.push_back(parameter.type);
            result.exports.push_back({value.name + ".new", FunctionName(value.name, "new"), value.name, std::move(constructorParameters)});
            for (const auto& function : value.functions) {
                if (function.isConstructor) continue;
                Line(0, [&] { MethodSignature(value, function); }, " {");
                functionReturn = Find(function.returnType);
                Statements(function.body, 1, value.name); Line(0, '}');
                functionReturn = TypeTable::Unknown;
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
