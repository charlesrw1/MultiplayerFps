#include "Semantic.h"
#include "Builtins.h"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

namespace luasharp {

TypeTable::TypeTable() {
    for (const auto& [kind, name] : {std::pair{TypeKind::Unknown, ""}, {TypeKind::Void, "void"}, {TypeKind::Null, "null"}, {TypeKind::Any, "any"}, {TypeKind::Table, "table"},
                                     {TypeKind::Int, "int"}, {TypeKind::Float, "float"}, {TypeKind::Bool, "bool"}, {TypeKind::String, "string"}, {TypeKind::Range, "$range"}}) Add(kind, name);
    assert(Name(Range) == "$range");
}

TypeId TypeTable::Add(TypeKind kind, std::string name, TypeId element, uint32_t arity) {
    const auto id = static_cast<TypeId>(entries_.size());
    byName_.emplace(name, id);
    entries_.push_back({kind, std::move(name), element, arity});
    return id;
}

TypeId TypeTable::Composite(TypeKind kind, TypeId element, uint32_t arity, std::string name) {
    const auto key = (static_cast<uint64_t>(kind) << 56) | (static_cast<uint64_t>(arity) << 32) | element;
    if (const auto found = composites_.find(key); found != composites_.end()) return found->second;
    const auto id = Add(kind, std::move(name), element, arity);
    composites_.emplace(key, id);
    return id;
}

std::optional<TypeId> TypeTable::Find(std::string_view spelling) const {
    const auto found = byName_.find(spelling);
    return found == byName_.end() ? std::nullopt : std::optional<TypeId>(found->second);
}

TypeId TypeTable::Intern(std::string_view spelling) {
    if (const auto found = byName_.find(spelling); found != byName_.end()) return found->second;
    if (spelling.ends_with("[]")) return List(Intern(spelling.substr(0, spelling.size() - 2)));
    if (spelling.starts_with("$type:")) return Meta(spelling.substr(6));
    if (spelling.starts_with("$builtin:")) return Builtin(spelling.substr(9));
    if (spelling.starts_with("$function:")) {
        const auto separator = spelling.find(':', 10);
        size_t arity{};
        for (const char digit : spelling.substr(10, separator - 10)) arity = arity * 10 + static_cast<size_t>(digit - '0');
        return Function(arity, separator == std::string_view::npos ? Any : Intern(spelling.substr(separator + 1)));
    }
    return Add(TypeKind::Named, std::string(spelling));
}

TypeId TypeTable::List(TypeId element) {
    return Composite(TypeKind::List, element, 0, Name(element) + "[]");
}

TypeId TypeTable::Function(size_t arity, TypeId result) {
    assert(arity <= std::numeric_limits<uint16_t>::max());
    return Composite(TypeKind::Function, result, static_cast<uint32_t>(arity), "$function:" + std::to_string(arity) + ":" + Name(result));
}

TypeId TypeTable::Meta(std::string_view name) {
    std::string spelling = "$type:"; spelling += name;
    if (const auto found = byName_.find(spelling); found != byName_.end()) return found->second;
    return Add(TypeKind::Meta, std::move(spelling));
}

TypeId TypeTable::Builtin(std::string_view name) {
    std::string spelling = "$builtin:"; spelling += name;
    if (const auto found = byName_.find(spelling); found != byName_.end()) return found->second;
    return Add(TypeKind::Builtin, std::move(spelling));
}

std::string_view TypeTable::Target(TypeId id) const {
    const auto& entry = At(id);
    assert(entry.kind == TypeKind::Meta || entry.kind == TypeKind::Builtin);
    return std::string_view(entry.name).substr(entry.kind == TypeKind::Meta ? 6 : 9);
}

std::string_view LoweringName(Lowering lowering) {
    switch (lowering) {
    case Lowering::None: return "none";
    case Lowering::ImplicitField: return "implicit_field";
    case Lowering::ImplicitMethod: return "implicit_method";
    case Lowering::StaticCall: return "static_call";
    case Lowering::StaticField: return "static_field";
    case Lowering::Base: return "base";
    case Lowering::NativeMethod: return "native_method";
    case Lowering::NativeStatic: return "native_static";
    case Lowering::NativeConstant: return "native_constant";
    case Lowering::Construct: return "construct";
    case Lowering::Printf: return "printf";
    case Lowering::Concat: return "concat";
    case Lowering::CompoundConcat: return "compound_concat";
    case Lowering::ListIndex: return "list_index";
    case Lowering::ListSet: return "list_set";
    case Lowering::ListPushBack: return "list_push_back";
    case Lowering::ListPopBack: return "list_pop_back";
    case Lowering::ListSize: return "list_size";
    case Lowering::ListEmpty: return "list_empty";
    case Lowering::ListClear: return "list_clear";
    case Lowering::StringLength: return "string_length";
    case Lowering::StringContains: return "string_contains";
    case Lowering::StringStartsWith: return "string_starts_with";
    case Lowering::StringEndsWith: return "string_ends_with";
    case Lowering::StringSubstring: return "string_substring";
    case Lowering::StringLower: return "string_lower";
    case Lowering::StringUpper: return "string_upper";
    case Lowering::CastIdentity: return "cast_identity";
    case Lowering::CastString: return "cast_string";
    case Lowering::CastBoolNumber: return "cast_bool_number";
    case Lowering::CastBoolString: return "cast_bool_string";
    case Lowering::CastIntFloat: return "cast_int_float";
    case Lowering::CastNumberBool: return "cast_number_bool";
    case Lowering::CastNumberString: return "cast_number_string";
    case Lowering::CastDynamic: return "cast_dynamic";
    }
    assert(false);
    return {};
}

namespace {

struct Local {
    std::string_view name;
    TypeId type{};
    size_t symbolIndex{};
    size_t depth{};
};

constexpr TypeId InferReturn = std::numeric_limits<TypeId>::max(); // lambda bodies infer their return type

class Analyzer {
public:
    Analyzer(std::string_view source, SyntaxTree syntax, const std::vector<ExternalType>& externalTypes = {}) : source_(source), types_(result_.types) {
        lineStarts_.push_back(0);
        for (size_t at{}; (at = source_.find('\n', at)) != std::string_view::npos; ++at) lineStarts_.push_back(at + 1);
        result_.syntax = std::move(syntax);
        // Estimates from token count; the pools still grow if a program exceeds them.
        result_.bound.expressions.reserve(result_.syntax.tokens.size() + 1);
        result_.bound.statements.reserve(result_.syntax.tokens.size() / 2);
        result_.bound.expressions.emplace_back(); // shared missing expression
        for (const auto& type : BuiltinTypes()) knownTypes_.insert(type);
        for (const auto& type : NativeStructTypes()) structTypes_.insert(type);
        RegisterBuiltinType("string"); RegisterBuiltinType("math");
        for (const auto& type : NativeStructTypes()) RegisterBuiltinType(type);
        for (const auto& external : externalTypes) {
            knownTypes_.insert(external.name); declaredTypes_.insert(external.name);
            if (external.isEnum) enumTypes_.insert(external.name);
            if (external.isStruct) structTypes_.insert(external.name);
            Symbol typeSymbol{external.name, external.name, external.isEnum ? SymbolKind::Enum : SymbolKind::Class, 0, 0, {source_.size(), 0}, {}, external.name, "public"};
            typeSymbol.isStruct = external.isStruct; typeSymbol.isNative = external.isNative;
            if (external.isNative) nativeTypes_.insert(external.name);
            PushSymbol(std::move(typeSymbol));
            if (!external.baseType.empty()) result_.baseTypes[external.name] = external.baseType;
            for (auto member : external.members) {
                member.span = {source_.size(), 0}; member.implicit = true;
                const bool constructor = member.kind == SymbolKind::Method && member.name == external.name;
                const auto staticMember = member.isStatic;
                const auto index = PushSymbol(std::move(member));
                if (constructor) constructors_[external.name].push_back(index);
                else result_.members[external.isEnum || staticMember ? "$type:" + external.name : external.name].push_back(index);
            }
        }
    }

    Analysis Run() {
        for (const auto& diagnostic : result_.syntax.diagnostics) AddDiagnostic(diagnostic.message, diagnostic.span);
        DeclareTypes();
        DeclareMembers();
        FlattenInheritance();
        CheckOverrides();
        AnalyzeBodies();
        return std::move(result_);
    }

private:
    bool IsNumeric(TypeId type) const { return type == TypeTable::Int || type == TypeTable::Float; }
    bool IsOpen(TypeId type) const { return type == TypeTable::Table || type == TypeTable::Any; }
    const std::string& Name(TypeId type) const { return types_.Name(type); }
    const SyntaxTree& Syntax() const { return result_.syntax; }
    std::pair<size_t, size_t> Position(size_t offset) const {
        offset = (std::min)(offset, source_.size());
        const auto next = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), offset);
        assert(next != lineStarts_.begin());
        const auto line = static_cast<size_t>(next - lineStarts_.begin()) - 1;
        return {line, offset - lineStarts_[line]};
    }
    size_t PushSymbol(Symbol symbol) {
        const auto index = result_.symbols.size();
        symbolTypes_.push_back(types_.Intern(symbol.type));
        result_.symbols.push_back(std::move(symbol));
        return index;
    }
    size_t AddSymbol(std::string name, std::string type, SymbolKind kind, TextSpan span, std::vector<std::string> parameters = {}) {
        const auto [line, character] = Position(span.offset);
        return PushSymbol({std::move(name), std::move(type), kind, line, character, span, std::move(parameters)});
    }
    void AddDiagnostic(std::string message, TextSpan span, DiagnosticSeverity severity = DiagnosticSeverity::Error) {
        const auto [line, character] = Position(span.offset); result_.diagnostics.push_back({std::move(message), line, character, severity, span});
    }
    void CheckType(TypeId type, TextSpan span) {
        const auto base = types_.Base(type);
        if (base != TypeTable::Unknown && base != TypeTable::Void && !knownTypes_.contains(Name(base))) AddDiagnostic("Unknown type '" + Name(base) + "'", span);
        if (types_.IsList(type)) RegisterBuiltinType(Name(type));
    }
    static std::vector<std::string> ParameterLabels(const BuiltinMember& member) {
        std::vector<std::string> result; for (const auto& parameter : member.parameters) result.push_back(parameter.type + " " + parameter.name + (parameter.optional ? "?" : "")); return result;
    }
    void RegisterBuiltinType(std::string type) { // by value: interning below can move TypeTable names
        if (!registeredBuiltins_.insert(type).second) return;
        const auto add = [&](const BuiltinMember& member, const std::string& key) {
            Symbol symbol{member.name, member.returnType, member.kind, 0, 0, {source_.size(), 0}, ParameterLabels(member), type};
            symbol.implicit = true; symbol.isStatic = member.isStatic;
            const auto index = PushSymbol(std::move(symbol));
            result_.members[key].push_back(index);
            symbolBuiltins_.emplace(index, &member);
        };
        for (const auto* member : BuiltinMembers(type, false)) add(*member, type);
        for (const auto* member : BuiltinMembers(type, true)) add(*member, "$type:" + type);
    }
    TextSpan ParameterNameSpan(const SyntaxParameter& parameter) const {
        return {parameter.span.End() - parameter.name.size(), parameter.name.size()};
    }
    void DeclareTypes() {
        for (const auto& value : result_.syntax.root.enums) {
            if (!declaredTypes_.insert(value.name).second) AddDiagnostic("Type '" + value.name + "' is already declared", value.nameSpan);
            knownTypes_.insert(value.name); enumTypes_.insert(value.name);
            const auto index = AddSymbol(value.name, value.name, SymbolKind::Enum, value.nameSpan); result_.symbols[index].owner = value.name;
        }
        for (const auto& value : result_.syntax.root.classes) {
            if (IsBuiltinType(value.name) || IsBuiltinStaticClass(value.name)) AddDiagnostic("'" + value.name + "' is a built-in type and cannot be redeclared", value.span);
            else if (!declaredTypes_.insert(value.name).second) AddDiagnostic("Type '" + value.name + "' is already declared", value.span);
            knownTypes_.insert(value.name); classes_.try_emplace(value.name, &value);
            if (value.isStruct) structTypes_.insert(value.name);
            const auto index = AddSymbol(value.name, value.name, SymbolKind::Class, value.span);
            auto& symbol = result_.symbols[index];
            symbol.owner = value.name; symbol.isStruct = value.isStruct; symbol.isAbstract = value.isAbstract; symbol.isSealed = value.isSealed; symbol.isNative = value.isExtern;
            if (value.isExtern) nativeTypes_.insert(value.name);
            if (value.isStruct && (value.isAbstract || value.isSealed)) AddDiagnostic("Structs cannot be 'abstract' or 'sealed'", value.span);
            if (value.isAbstract && value.isSealed) AddDiagnostic("Class '" + value.name + "' cannot be both 'abstract' and 'sealed'", value.span);
            if (!value.baseName.empty()) result_.baseTypes[value.name] = value.baseName;
        }
        for (const auto& value : result_.syntax.root.classes) {
            if (value.baseName.empty()) continue;
            CheckType(types_.Intern(value.baseName), value.span);
            if (structTypes_.contains(value.baseName) || enumTypes_.contains(value.baseName)) AddDiagnostic("Class '" + value.name + "' cannot derive from " + (enumTypes_.contains(value.baseName) ? "enum" : "struct") + " '" + value.baseName + "'", value.span);
            else if (const auto* base = FindClass(value.baseName); base && base->isSealed) AddDiagnostic("Class '" + value.name + "' cannot derive from sealed class '" + value.baseName + "'", value.span);
            else if (value.isExtern && knownTypes_.contains(value.baseName) && !nativeTypes_.contains(value.baseName)) AddDiagnostic("Extern class '" + value.name + "' can only derive from an extern class", value.span);
        }
    }
    void DeclareMembers() {
        for (const auto& value : result_.syntax.root.enums) {
            std::unordered_set<std::string_view> names;
            if (value.members.empty()) AddDiagnostic("Enum '" + value.name + "' must declare at least one member", value.span);
            auto& members = result_.members["$type:" + value.name];
            for (const auto& member : value.members) {
                if (!names.insert(member.name).second) AddDiagnostic("Enum member '" + member.name + "' is already declared on " + value.name, member.nameSpan);
                const auto index = AddSymbol(member.name, value.name, SymbolKind::Field, member.nameSpan);
                result_.symbols[index].owner = value.name; members.push_back(index);
            }
        }
        for (const auto& value : result_.syntax.root.classes) {
            struct Declared { std::string_view name; bool method; std::vector<std::string> parameterTypes; };
            std::vector<Declared> declared;
            auto& members = result_.members[value.name];
            auto& statics = result_.members["$type:" + value.name];
            auto& constructors = constructors_[value.name];
            for (const auto& field : value.fields) {
                CheckType(types_.Intern(field.type), field.span);
                if (field.type == "void") AddDiagnostic("Field '" + field.name + "' cannot be 'void'", field.span);
                if (std::any_of(declared.begin(), declared.end(), [&](const Declared& item) { return item.name == field.name; })) AddDiagnostic("Member '" + field.name + "' is already declared on " + value.name, field.span);
                declared.push_back({field.name, false, {}});
                if (value.isExtern && field.isStatic) AddDiagnostic("Extern class '" + value.name + "' cannot declare static field '" + field.name + "'", field.span);
                if (value.isExtern && field.initializer) AddDiagnostic("Extern field '" + field.name + "' cannot have an initializer", field.span);
                const auto index = AddSymbol(field.name, field.type, SymbolKind::Field, field.span);
                result_.symbols[index].owner = value.name; result_.symbols[index].access = field.access; result_.symbols[index].isStatic = field.isStatic;
                (field.isStatic ? statics : members).push_back(index);
            }
            for (const auto& function : value.functions) {
                if (function.isConstructor && function.name != value.name) AddDiagnostic("Constructor name must match " + std::string(value.isStruct ? "struct" : "class") + " '" + value.name + "'", function.span);
                if (!function.isConstructor) CheckType(types_.Intern(function.returnType), function.span);
                std::vector<std::string> parameters, parameterTypes;
                for (const auto& parameter : function.parameters) { CheckType(types_.Intern(parameter.type), ParameterNameSpan(parameter)); parameters.push_back(parameter.type + " " + parameter.name); parameterTypes.push_back(parameter.type); }
                if (function.isConstructor) {
                    const bool duplicate = std::any_of(constructors.begin(), constructors.end(), [&](size_t other) { return SameParameterTypes(result_.symbols[other].parameters, parameters); });
                    if (duplicate) AddDiagnostic("Constructor with the same parameters is already declared on " + value.name, function.span);
                    else if (value.isExtern && !constructors.empty()) AddDiagnostic("Extern class '" + value.name + "' can declare only one constructor", function.span);
                } else {
                    const bool duplicate = std::any_of(declared.begin(), declared.end(), [&](const Declared& item) { return item.name == function.name && (!item.method || item.parameterTypes == parameterTypes); });
                    if (duplicate) AddDiagnostic("Member '" + function.name + "' is already declared on " + value.name, function.span);
                    else if (value.isExtern && std::any_of(declared.begin(), declared.end(), [&](const Declared& item) { return item.name == function.name; }))
                        AddDiagnostic("Extern method '" + function.name + "' cannot be overloaded", function.span);
                    declared.push_back({function.name, true, parameterTypes});
                }
                const auto index = AddSymbol(function.name, function.isConstructor ? value.name : function.returnType, SymbolKind::Method, function.span, std::move(parameters));
                auto& symbol = result_.symbols[index];
                const auto& modifiers = function.modifiers;
                symbol.owner = value.name; symbol.access = function.access; symbol.isStatic = function.isStatic;
                symbol.isVirtual = modifiers.isVirtual || modifiers.isOverride || modifiers.isAbstract;
                symbol.isOverride = modifiers.isOverride; symbol.isAbstract = modifiers.isAbstract; symbol.isSealed = modifiers.isSealed;
                CheckMethodModifiers(value, function);
                functionSymbols_.emplace(&function, index);
                if (function.isConstructor) constructors.push_back(index);
                else (function.isStatic ? statics : members).push_back(index);
            }
        }
        CheckStructCycles();
    }
    static bool SameParameterTypes(const std::vector<std::string>& left, const std::vector<std::string>& right) {
        if (left.size() != right.size()) return false;
        for (size_t i{}; i < left.size(); ++i) if (std::string_view(left[i]).substr(0, left[i].find(' ')) != std::string_view(right[i]).substr(0, right[i].find(' '))) return false;
        return true;
    }
    void CheckMethodModifiers(const SyntaxClass& owner, const SyntaxFunction& function) {
        const auto& modifiers = function.modifiers;
        const bool dispatched = modifiers.isVirtual || modifiers.isOverride || modifiers.isAbstract;
        const auto name = "'" + function.name + "'";
        if (owner.isExtern) {
            if (function.hasBody) AddDiagnostic("Extern method " + name + " cannot have a body", function.span);
            if (function.hasBaseCall) AddDiagnostic("Extern constructor of '" + owner.name + "' cannot call ': base(...)'", function.span);
            if (modifiers.isOverride || modifiers.isAbstract || modifiers.isSealed) AddDiagnostic("Extern method " + name + " can only be 'static' or 'virtual'", function.span);
            return;
        }
        if (function.isConstructor) { if (!function.hasBody) AddDiagnostic("Constructor of '" + owner.name + "' must have a body", function.span); return; }
        if (owner.isStruct && (dispatched || modifiers.isSealed)) AddDiagnostic("Struct method " + name + " cannot be virtual, override, abstract or sealed", function.span);
        else if (function.isStatic && (dispatched || modifiers.isSealed)) AddDiagnostic("Static method " + name + " cannot be virtual, override, abstract or sealed", function.span);
        if (int(modifiers.isVirtual) + int(modifiers.isOverride) + int(modifiers.isAbstract) > 1) AddDiagnostic("Method " + name + " can only be one of 'virtual', 'override' and 'abstract'", function.span);
        if (modifiers.isSealed && !modifiers.isOverride) AddDiagnostic("Only an 'override' method can be 'sealed' (" + name + ")", function.span);
        if (dispatched && function.access == "private") AddDiagnostic("Virtual method " + name + " cannot be private", function.span);
        if (modifiers.isAbstract && function.hasBody) AddDiagnostic("Abstract method " + name + " cannot have a body", function.span);
        if (!modifiers.isAbstract && !function.hasBody) AddDiagnostic("Method " + name + " must have a body unless it is 'abstract'", function.span);
        if (modifiers.isAbstract && !owner.isAbstract && !owner.isStruct) AddDiagnostic("Class '" + owner.name + "' must be 'abstract' to declare abstract method " + name, function.span);
    }
    // Uses the flattened member lists: members[Base] holds everything Base declares or inherits.
    void CheckOverrides() {
        for (const auto& value : result_.syntax.root.classes) {
            if (value.isStruct) continue;
            const auto base = result_.baseTypes.find(value.name);
            const auto inherited = base == result_.baseTypes.end() ? result_.members.end() : result_.members.find(base->second);
            const auto own = result_.members.find(value.name);
            if (own == result_.members.end()) continue;
            for (const auto index : own->second) {
                const auto& mine = result_.symbols[index];
                if (mine.kind != SymbolKind::Method) continue;
                if (mine.owner != value.name) {
                    if (mine.isAbstract && !value.isAbstract) AddDiagnostic("Class '" + value.name + "' must be 'abstract' or override abstract method '" + mine.owner + "." + mine.name + "'", value.span);
                    continue;
                }
                const Symbol* match = nullptr;
                if (inherited != result_.members.end()) for (const auto other : inherited->second) {
                    const auto& candidate = result_.symbols[other];
                    if (candidate.kind == SymbolKind::Method && !candidate.isStatic && candidate.access != "private" && candidate.name == mine.name && SameParameterTypes(candidate.parameters, mine.parameters)) { match = &candidate; break; }
                }
                // Members of external types carry no modifiers; they can always be overridden.
                const bool overridable = match && (match->isVirtual || match->implicit);
                const auto where = "Method '" + mine.name + "' on '" + value.name + "'";
                if (mine.isOverride) {
                    if (!match) AddDiagnostic(where + " is marked 'override', but no inherited virtual method has the same parameters", mine.span);
                    else if (!overridable) AddDiagnostic(where + " cannot override '" + match->owner + "." + mine.name + "', which is not virtual, abstract or override", mine.span);
                    else if (match->isSealed) AddDiagnostic(where + " cannot override sealed method '" + match->owner + "." + mine.name + "'", mine.span);
                } else if (match && !mine.isStatic) {
                    if (overridable) AddDiagnostic(where + " must be marked 'override' to replace inherited virtual method '" + match->owner + "." + mine.name + "'", mine.span);
                    else AddDiagnostic(where + " hides inherited method '" + match->owner + "." + mine.name + "', which is not virtual", mine.span);
                }
            }
        }
    }
    bool IsStructType(TypeId type) const { return structTypes_.contains(Name(type)); }
    // A static field declared in this source; it lives in module state.
    bool IsUserStaticField(size_t index) const {
        const auto& symbol = result_.symbols[index];
        return symbol.kind == SymbolKind::Field && symbol.isStatic && !symbol.implicit && classes_.contains(symbol.owner);
    }
    bool IsValueType(TypeId type) const { return IsNumeric(type) || type == TypeTable::Bool || enumTypes_.contains(Name(type)) || IsStructType(type); }
    // A struct cannot contain itself, directly or through other structs. Static fields are not part of the value.
    void CheckStructCycles() {
        StringSet done;
        const auto visit = [&](auto&& self, const SyntaxClass& value, std::vector<std::string_view>& path) -> void {
            if (std::find(path.begin(), path.end(), value.name) != path.end()) { AddDiagnostic("Struct '" + value.name + "' cannot contain itself", value.span); return; }
            if (!done.insert(value.name).second) return;
            path.push_back(value.name);
            for (const auto& field : value.fields) if (const auto* inner = FindClass(field.type); !field.isStatic && inner && inner->isStruct) self(self, *inner, path);
            path.pop_back();
        };
        for (const auto& value : result_.syntax.root.classes) if (value.isStruct) { std::vector<std::string_view> path; visit(visit, value, path); }
    }
    void FlattenInheritance() {
        for (const auto& [derived, firstBase] : result_.baseTypes) {
            for (const std::string prefix : {std::string(), std::string("$type:")}) {
                std::unordered_set<std::string_view> visited{derived}; std::string_view base = firstBase;
                auto& derivedMembers = result_.members[prefix + derived];
                while (!base.empty()) {
                    if (!visited.insert(base).second) { if (prefix.empty()) AddDiagnostic("Inheritance cycle involving '" + derived + "'", ClassSpan(derived)); break; }
                    if (const auto found = result_.members.find(prefix + std::string(base)); found != result_.members.end()) {
                        for (const auto memberIndex : std::vector<size_t>(found->second)) {
                            const auto& member = result_.symbols[memberIndex];
                            // Same name and, for methods, same parameters is an override; a different parameter list is an overload.
                            const auto own = std::find_if(derivedMembers.begin(), derivedMembers.end(), [&](size_t index) {
                                const auto& mine = result_.symbols[index];
                                return mine.name == member.name && (mine.kind != SymbolKind::Method || member.kind != SymbolKind::Method || SameParameterTypes(mine.parameters, member.parameters));
                            });
                            if (own == derivedMembers.end()) { derivedMembers.push_back(memberIndex); continue; }
                            const auto& mine = result_.symbols[*own];
                            // A derived object stores its base's fields too, so field names cannot repeat.
                            if (mine.kind == SymbolKind::Field && member.kind == SymbolKind::Field && !member.isStatic && !mine.isStatic) { if (prefix.empty() && mine.owner == derived) AddDiagnostic("Field '" + member.name + "' on '" + derived + "' conflicts with the field inherited from '" + member.owner + "'", mine.span); }
                            else if (member.access != "private" && (mine.kind != member.kind || mine.type != member.type || mine.parameters != member.parameters)) AddDiagnostic("Override of '" + member.name + "' on '" + derived + "' does not match the inherited signature", mine.span);
                        }
                    }
                    const auto parent = result_.baseTypes.find(base); base = parent == result_.baseTypes.end() ? std::string_view{} : std::string_view(parent->second);
                }
            }
        }
    }
    const SyntaxClass* FindClass(std::string_view name) const {
        const auto found = classes_.find(name);
        return found == classes_.end() ? nullptr : found->second;
    }
    TextSpan ClassSpan(std::string_view name) const {
        const auto* found = FindClass(name);
        return found ? found->span : TextSpan{};
    }
    // Every member called name on type, overloads in declaration order.
    std::vector<size_t> FindMembers(std::string_view type, std::string_view name) const {
        std::vector<size_t> result;
        auto found = result_.members.find(type);
        if (found == result_.members.end()) found = result_.members.find(type.substr(0, type.find("[]")));
        if (found == result_.members.end()) return result;
        for (const auto index : found->second) if (result_.symbols[index].name == name) result.push_back(index);
        return result;
    }
    std::optional<size_t> FindMember(std::string_view type, std::string_view name) const {
        auto found = result_.members.find(type);
        if (found == result_.members.end()) found = result_.members.find(type.substr(0, type.find("[]")));
        if (found == result_.members.end()) return std::nullopt;
        const auto member = std::find_if(found->second.begin(), found->second.end(), [&](size_t index) { return result_.symbols[index].name == name; });
        return member == found->second.end() ? std::nullopt : std::optional<size_t>(*member);
    }
    BoundExpression& Node(NodeId id) { assert(id < result_.bound.expressions.size()); return result_.bound.expressions[id]; }
    TypeId ChildType(NodeId id, size_t index) { const auto& node = Node(id); assert(index < node.children.count); return Node(node.children.first + static_cast<NodeId>(index)).type; }
    NodeId AllocateExpressions(size_t count) {
        auto& pool = result_.bound.expressions;
        const auto first = static_cast<NodeId>(pool.size());
        pool.resize(pool.size() + count);
        return first;
    }
    // One overload of a method, constructor, static function or native member.
    struct Callable {
        std::vector<TypeId> parameters;
        size_t required{};
        TypeId result{};
        std::optional<size_t> symbol;            // null for the implicit default constructor
        const BuiltinMember* builtin{};
        bool isStatic{};
    };
    Callable MakeCallable(size_t symbolIndex) {
        const auto& symbol = result_.symbols[symbolIndex];
        Callable callable; callable.symbol = symbolIndex; callable.result = symbolTypes_[symbolIndex]; callable.isStatic = symbol.isStatic;
        for (const auto& label : symbol.parameters) {
            auto type = std::string_view(label).substr(0, label.find(' '));
            if (type.ends_with('?')) type.remove_suffix(1);
            callable.parameters.push_back(types_.Intern(type));
        }
        callable.required = callable.parameters.size();
        if (const auto builtin = symbolBuiltins_.find(symbolIndex); builtin != symbolBuiltins_.end()) {
            callable.builtin = builtin->second;
            callable.required = static_cast<size_t>(std::count_if(builtin->second->parameters.begin(), builtin->second->parameters.end(), [](const auto& parameter) { return !parameter.optional; }));
        }
        return callable;
    }
    std::string Signature(std::string_view name, const Callable& callable) const {
        std::string text(name); text += '(';
        for (size_t i{}; i < callable.parameters.size(); ++i) { if (i) text += ", "; text += Name(callable.parameters[i]); if (i >= callable.required) text += '?'; }
        return text + ')';
    }
    // Cost of passing an argument of type actual for a parameter of type expected; negative when it cannot convert.
    int ConversionCost(TypeId expected, TypeId actual) const {
        if (expected == actual || expected == TypeTable::Unknown || actual == TypeTable::Unknown) return 0;
        if (!Assignable(expected, actual)) return -1;
        if (actual == TypeTable::Any) return expected == TypeTable::Int ? 21 : 20;
        if (expected == TypeTable::Any) return 10;
        if (actual == TypeTable::Null) return 5;
        if (expected == TypeTable::Float && actual == TypeTable::Int) return 2;
        if (types_.IsList(expected) || expected == TypeTable::Table) return 4;
        return 1; // derived to base
    }
    // Total conversion cost of the arguments for one overload, or -1 when it does not apply.
    int Score(const Callable& callable, NodeId arguments, size_t count) {
        if (count < callable.required || count > callable.parameters.size()) return -1;
        int total = static_cast<int>(callable.parameters.size() - count);
        for (size_t i{}; i < count; ++i) {
            const auto cost = ConversionCost(callable.parameters[i], Node(arguments + static_cast<NodeId>(i)).type);
            if (cost < 0) return -1;
            total += cost;
        }
        return total;
    }
    // Picks the cheapest applicable overload. Reports the failure and returns -1 when none applies.
    // What names the callee in messages: "constructor" gets the constructor wording. argumentSpan(i) locates argument i.
    using ArgumentSpan = std::function<TextSpan(size_t)>;
    int ResolveOverload(std::string_view name, const std::vector<Callable>& candidates, NodeId arguments, size_t count, TextSpan callSpan, const ArgumentSpan& argumentSpan, bool constructor) {
        int best = -1, bestCost = 0; bool tie = false;
        for (size_t i{}; i < candidates.size(); ++i) {
            const auto cost = Score(candidates[i], arguments, count);
            if (cost < 0) continue;
            if (best < 0 || cost < bestCost) { best = static_cast<int>(i); bestCost = cost; tie = false; }
            else if (cost == bestCost) tie = true;
        }
        if (best >= 0) {
            if (tie) {
                bool dynamic = false;
                for (size_t i{}; i < count; ++i) { const auto type = Node(arguments + static_cast<NodeId>(i)).type; dynamic = dynamic || type == TypeTable::Any || type == TypeTable::Unknown; }
                if (!dynamic) AddDiagnostic("Call to '" + std::string(name) + "' is ambiguous between overloads with equally good conversions", callSpan);
            }
            return best;
        }
        if (candidates.size() == 1) {
            const auto& only = candidates.front();
            if (count < only.required || count > only.parameters.size()) {
                const auto range = only.required == only.parameters.size() ? "" : " to " + std::to_string(only.parameters.size());
                AddDiagnostic("Expected " + std::to_string(only.required) + range + (constructor ? " constructor" : "") + " argument(s) for '" + std::string(name) + "', got " + std::to_string(count), callSpan);
                return -1;
            }
            for (size_t i{}; i < count; ++i) {
                const auto actual = Node(arguments + static_cast<NodeId>(i)).type;
                if (ConversionCost(only.parameters[i], actual) < 0)
                    AddDiagnostic(std::string(constructor ? "Constructor argument " : "Argument ") + std::to_string(i + 1) + (constructor ? " for '" : " of '") + std::string(name) + "' expects '" + Name(only.parameters[i]) + "', got " + DisplayType(actual), argumentSpan(i));
            }
            return -1;
        }
        std::string message = "No overload of '" + std::string(name) + "' accepts (";
        for (size_t i{}; i < count; ++i) { if (i) message += ", "; message += Name(Node(arguments + static_cast<NodeId>(i)).type); }
        message += "). Candidates: ";
        for (size_t i{}; i < candidates.size(); ++i) { if (i) message += "; "; message += Signature(name, candidates[i]); }
        AddDiagnostic(std::move(message), callSpan);
        return -1;
    }
    // Spans of the arguments of a call (child 0 is the callee) or of `new T(...)`.
    ArgumentSpan CallArgumentSpans(const SyntaxExpression& call) const {
        const size_t first = call.kind == SyntaxExpression::Kind::New ? 0 : 1;
        return [this, &call, first](size_t index) { return Syntax().Child(call, index + first).span; };
    }
    // Constructors of a type: declared overloads, plus the implicit default when no declared one takes no arguments.
    std::vector<Callable> ConstructorCandidates(const std::string& name) {
        std::vector<Callable> result;
        if (IsNativeStruct(name)) {
            for (const auto index : FindMembers("$type:" + name, "new")) result.push_back(MakeCallable(index));
            return result;
        }
        const auto declared = constructors_.find(name);
        if (declared != constructors_.end()) for (const auto index : declared->second) result.push_back(MakeCallable(index));
        const bool inSource = FindClass(name) != nullptr && !nativeTypes_.contains(name); // extern classes have no implicit constructor
        const bool hasDefault = std::any_of(result.begin(), result.end(), [](const Callable& item) { return item.parameters.empty(); });
        const bool defaultAllowed = structTypes_.contains(name) ? !hasDefault : result.empty();
        if (inSource && defaultAllowed) { Callable fallback; fallback.result = types_.Intern(name); result.push_back(fallback); }
        return result;
    }
    void CoerceArguments(const Callable& callable, NodeId arguments, size_t count) {
        for (size_t i{}; i < count && i < callable.parameters.size(); ++i) Accepts(callable.parameters[i], arguments + static_cast<NodeId>(i));
    }
    void ApplyNative(const Callable& chosen, bool constructing, Lowering& lowering, const BuiltinMember*& nativeMember) {
        nativeMember = chosen.builtin;
        lowering = chosen.builtin->lowering;
        if (lowering == Lowering::None && !chosen.builtin->cName.empty()) lowering = constructing ? Lowering::Construct : chosen.builtin->isStatic ? Lowering::NativeStatic : Lowering::NativeMethod;
    }
    // `new T(...)` for a class, or for a struct (which still produces a value).
    void ResolveConstruction(const SyntaxExpression& expression, NodeId arguments, size_t count, TypeId& type, std::optional<size_t>& symbol, Lowering& lowering, const BuiltinMember*& nativeMember) {
        if (const auto* declared = FindClass(expression.text); declared && declared->isAbstract) { AddDiagnostic("Cannot create an instance of abstract class '" + expression.text + "'", expression.span); return; }
        const auto candidates = ConstructorCandidates(expression.text);
        if (candidates.empty()) {
            if (nativeTypes_.contains(expression.text)) AddDiagnostic("Cannot create an instance of extern class '" + expression.text + "': it declares no constructor", expression.span);
            return;
        }
        const int pick = ResolveOverload(expression.text, candidates, arguments, count, expression.span, CallArgumentSpans(expression), true);
        if (pick < 0) return;
        const auto& chosen = candidates[static_cast<size_t>(pick)];
        symbol = chosen.symbol; type = chosen.result;
        if (chosen.builtin) ApplyNative(chosen, true, lowering, nativeMember);
        else if (structTypes_.contains(expression.text)) lowering = Lowering::Construct;
        CoerceArguments(chosen, arguments, count);
    }
    // Calls to methods, static functions, struct constructors (`Vec3(1, 2, 3)`) and native members. Sets the call's type, symbol and lowering.
    void ResolveCall(const SyntaxExpression& call, const SyntaxExpression& callee, NodeId calleeSlot, NodeId arguments, size_t argumentCount, const SyntaxClass* owner, TypeId& type, std::optional<size_t>& symbol, Lowering& lowering, const BuiltinMember*& nativeMember) {
        using Kind = SyntaxExpression::Kind;
        std::vector<Callable> candidates;
        bool staticAccess = false, constructing = false;
        std::string typeName;
        const auto methodsOf = [&](std::string_view key, std::string_view name) {
            for (const auto index : FindMembers(key, name)) if (result_.symbols[index].kind == SymbolKind::Method) candidates.push_back(MakeCallable(index));
        };
        if (callee.kind == Kind::Member) {
            const auto receiver = Node(Node(calleeSlot).children.first).type;
            if (types_.IsList(receiver)) RegisterBuiltinType(Name(receiver));
            staticAccess = types_.Kind(receiver) == TypeKind::Meta;
            typeName = staticAccess ? std::string(types_.Target(receiver)) : Name(receiver);
            methodsOf(Name(receiver), callee.text);
            if (candidates.empty()) methodsOf(staticAccess ? typeName : "$type:" + typeName, callee.text); // the other kind of method: misuse is reported below
        } else if (callee.kind == Kind::Identifier && types_.Kind(type) == TypeKind::Meta) {
            constructing = true; typeName = std::string(types_.Target(type));
            if (structTypes_.contains(typeName)) candidates = ConstructorCandidates(typeName);
            else if (FindClass(typeName)) { AddDiagnostic("Use 'new' to create an instance of class '" + typeName + "'", call.span); return; }
            else { AddDiagnostic("Type '" + typeName + "' cannot be called", callee.span); return; }
        } else if (callee.kind == Kind::Identifier && owner) {
            methodsOf(owner->name, callee.text); methodsOf("$type:" + owner->name, callee.text);
        }
        if (candidates.empty()) {
            if (type != TypeTable::Unknown && type != TypeTable::Any) AddDiagnostic("Expression of type " + DisplayType(type) + " is not callable", callee.span);
            return;
        }
        const int pick = ResolveOverload(constructing ? typeName : callee.text, candidates, arguments, argumentCount, call.span, CallArgumentSpans(call), constructing);
        if (pick < 0) { type = candidates.front().result; return; }
        const auto& chosen = candidates[static_cast<size_t>(pick)];
        type = chosen.result; symbol = chosen.symbol;
        if (callee.kind == Kind::Member && Node(Node(calleeSlot).children.first).lowering == Lowering::Base && chosen.symbol && result_.symbols[*chosen.symbol].isAbstract)
            AddDiagnostic("Cannot call abstract method '" + result_.symbols[*chosen.symbol].owner + "." + callee.text + "' through 'base'", callee.span);
        if (!constructing) {
            Node(calleeSlot).type = chosen.result;
            if (chosen.symbol) Node(calleeSlot).symbolIndex = chosen.symbol;
            if (callee.kind == Kind::Identifier) Node(calleeSlot).lowering = chosen.isStatic ? Lowering::StaticCall : Lowering::ImplicitMethod;
        }
        if (chosen.builtin) ApplyNative(chosen, constructing, lowering, nativeMember);
        else if (constructing) lowering = Lowering::Construct;
        else if (chosen.isStatic) lowering = Lowering::StaticCall;
        if (!constructing && callee.kind == Kind::Member) {
            if (staticAccess && !chosen.isStatic) AddDiagnostic("Instance method '" + callee.text + "' cannot be called on type '" + typeName + "'", callee.span);
            else if (!staticAccess && chosen.isStatic) AddDiagnostic("Static method '" + callee.text + "' must be called on its type, not on a value", callee.span);
        }
        if (!constructing && callee.kind == Kind::Identifier && !chosen.isStatic && staticContext_) AddDiagnostic("Instance method '" + callee.text + "' cannot be called from " + std::string(staticContextName_), callee.span);
        CoerceArguments(chosen, arguments, argumentCount);
    }
    // Lambdas keep a pointer to nothing on the stack, so they cannot capture a struct's `this`.
    void CheckStructLambdaCapture(const SyntaxClass* owner, TextSpan span) {
        if (lambdaNesting_ > 0 && owner && owner->isStruct) AddDiagnostic("Lambdas inside struct methods cannot use 'this' or its members", span);
    }
    // True when the bound expression names storage, so its members can be assigned and its address taken.
    bool IsAddressable(NodeId id) {
        const auto& node = Node(id);
        switch (node.kind) {
        case SyntaxExpression::Kind::Identifier: return types_.Kind(node.type) != TypeKind::Meta;
        case SyntaxExpression::Kind::Parenthesized: return IsAddressable(node.children.first);
        case SyntaxExpression::Kind::Member: {
            if (node.lowering == Lowering::NativeConstant) return false;
            const auto receiver = Node(node.children.first).type;
            if (nativeTypes_.contains(Name(receiver))) return false; // the host reads and writes extern fields through accessors
            return IsStructType(receiver) ? IsAddressable(node.children.first) : true;
        }
        case SyntaxExpression::Kind::Index: return node.lowering == Lowering::ListIndex;
        default: return false;
        }
    }
    void CheckBuiltinArguments(std::string_view name, std::span<const BuiltinParameter> parameters, bool variadic, const SyntaxExpression& call, NodeId arguments, size_t count) {
        const size_t required = static_cast<size_t>(std::count_if(parameters.begin(), parameters.end(), [](const auto& value) { return !value.optional; }));
        if (count < required || (!variadic && count > parameters.size())) {
            const auto range = variadic ? " or more" : required == parameters.size() ? "" : " to " + std::to_string(parameters.size());
            AddDiagnostic("Expected " + std::to_string(required) + range + " argument(s) for '" + std::string(name) + "', got " + std::to_string(count), call.span);
            if (!variadic) return;
        }
        const auto checked = (std::min)(count, parameters.size());
        for (size_t i{}; i < checked; ++i) {
            const auto actual = Node(arguments + static_cast<NodeId>(i)).type;
            if (!Accepts(types_.Intern(parameters[i].type), arguments + static_cast<NodeId>(i))) AddDiagnostic("Argument " + std::to_string(i + 1) + " of '" + std::string(name) + "' expects '" + parameters[i].type + "', got " + DisplayType(actual), Syntax().Child(call, i + 1).span);
        }
    }
    const Local* FindLocal(const std::vector<Local>& locals, std::string_view name) const {
        const auto found = std::find_if(locals.rbegin(), locals.rend(), [&](const Local& value) { return value.name == name; });
        return found == locals.rend() ? nullptr : &*found;
    }
    // Binds expression into pool slot `slot`. Never holds a node reference across
    // a recursive call, since recursion grows the pool.
    TypeId Infer(const SyntaxExpression& expression, std::vector<Local>& locals, const SyntaxClass* owner, NodeId slot) {
        using Kind = SyntaxExpression::Kind;
        const bool baseAllowed = std::exchange(baseAllowed_, false); // only the target of a member access may be `base`
        const size_t childCount = expression.children.count;
        const NodeId first = AllocateExpressions(childCount);
        NodeId visited{};
        const auto syntaxChild = [&](size_t index) -> const SyntaxExpression& { return Syntax().Child(expression, index); };
        const auto child = [&](size_t index) {
            assert(index == visited && index < childCount);
            ++visited;
            return Infer(syntaxChild(index), locals, owner, first + static_cast<NodeId>(index));
        };
        TypeId type{}; std::optional<size_t> symbol; Lowering lowering{}; NodeRange lambdaBody;
        const BuiltinMember* nativeMember = nullptr; const BuiltinOperator* nativeOperator = nullptr;
        switch (expression.kind) {
        case Kind::Missing: break;
        case Kind::Number: type = expression.text.find('.') == std::string::npos ? TypeTable::Int : TypeTable::Float; break;
        case Kind::String: type = TypeTable::String; break;
        case Kind::Boolean: type = TypeTable::Bool; break;
        case Kind::Null: type = TypeTable::Null; break;
        case Kind::Table: type = TypeTable::Table; for (size_t i{}; i < childCount; ++i) child(i); break;
        case Kind::TableEntry:
            if (expression.text.empty()) { child(0); child(1); }
            else child(0);
            type = TypeTable::Any; break;
        case Kind::New: {
            type = types_.Intern(expression.text); CheckType(type, expression.span);
            for (size_t i{}; i < childCount; ++i) child(i);
            if (enumTypes_.contains(expression.text)) AddDiagnostic("Enum type '" + expression.text + "' cannot be constructed", expression.span);
            else if (IsBuiltinType(expression.text) && !IsNativeStruct(expression.text)) AddDiagnostic("Type '" + expression.text + "' cannot be constructed with 'new'", expression.span);
            else ResolveConstruction(expression, first, childCount, type, symbol, lowering, nativeMember);
            break;
        }
        case Kind::Identifier: {
            if (expression.text == "this" && staticContext_) AddDiagnostic("'this' cannot be used in " + std::string(staticContextName_), expression.span);
            if (const auto* local = FindLocal(locals, expression.text)) {
                type = local->type; symbol = local->symbolIndex;
                if (expression.text == "this") CheckStructLambdaCapture(owner, expression.span);
            }
            // `base` is not reserved: a local or member with that name wins.
            else if (expression.text == "base" && owner && !FindMember(owner->name, "base") && !FindMember("$type:" + owner->name, "base")) {
                if (owner->isStruct || owner->baseName.empty()) AddDiagnostic("'base' requires a base class", expression.span);
                else {
                    type = types_.Intern(owner->baseName); lowering = Lowering::Base;
                    if (staticContext_) AddDiagnostic("'base' cannot be used in " + std::string(staticContextName_), expression.span);
                    else if (!baseAllowed) AddDiagnostic("'base' can only be used to access a member, as in 'base.f()'", expression.span);
                }
            }
            else if (owner) {
                if (const auto index = FindMember(owner->name, expression.text)) {
                    const auto& member = result_.symbols[*index];
                    type = symbolTypes_[*index]; symbol = index; lowering = member.kind == SymbolKind::Method ? Lowering::ImplicitMethod : Lowering::ImplicitField;
                    if (member.access == "private" && member.owner != owner->name) AddDiagnostic("Member '" + member.name + "' is private on '" + member.owner + "'", expression.span);
                    if (staticContext_) AddDiagnostic("Instance member '" + member.name + "' cannot be used in " + std::string(staticContextName_), expression.span);
                    CheckStructLambdaCapture(owner, expression.span);
                }
                else if (const auto staticIndex = FindMember("$type:" + owner->name, expression.text)) {
                    const auto& member = result_.symbols[*staticIndex];
                    type = symbolTypes_[*staticIndex]; symbol = staticIndex; lowering = member.kind == SymbolKind::Method ? Lowering::StaticCall : IsUserStaticField(*staticIndex) ? Lowering::StaticField : Lowering::None;
                    if (member.access == "private" && member.owner != owner->name) AddDiagnostic("Member '" + member.name + "' is private on '" + member.owner + "'", expression.span);
                }
            }
            if (type == TypeTable::Unknown && (IsBuiltinType(expression.text) || IsBuiltinStaticClass(expression.text) || declaredTypes_.contains(expression.text))) type = types_.Meta(expression.text);
            if (type == TypeTable::Unknown) if (const auto* builtin = FindBuiltinFunction(expression.text)) { type = types_.Builtin(builtin->name); symbol = BuiltinSymbolIndex(*builtin); }
            if (type == TypeTable::Unknown) AddDiagnostic("Unknown identifier '" + expression.text + "'", expression.span);
            break;
        }
        case Kind::Parenthesized: type = child(0); break;
        case Kind::Member: {
            baseAllowed_ = true;
            const auto target = child(0);
            if (const auto index = FindMember(Name(target), expression.text)) {
                const auto& member = result_.symbols[*index];
                type = symbolTypes_[*index]; symbol = index;
                if (member.access == "private" && (!owner || member.owner != owner->name)) AddDiagnostic("Member '" + member.name + "' is private on '" + member.owner + "'", MemberNameSpan(expression));
                if (const auto builtin = symbolBuiltins_.find(*index); builtin != symbolBuiltins_.end() && builtin->second->isStatic && builtin->second->kind == SymbolKind::Field) { nativeMember = builtin->second; lowering = Lowering::NativeConstant; }
                else if (types_.Kind(target) == TypeKind::Meta && IsUserStaticField(*index)) lowering = Lowering::StaticField;
            }
            else if (IsOpen(target)) type = TypeTable::Any;
            else if (expression.text != "<missing>" && target != TypeTable::Unknown) AddDiagnostic("Unknown member '" + expression.text + "' on " + Name(types_.Base(target)), MemberNameSpan(expression), DiagnosticSeverity::Warning);
            break;
        }
        case Kind::Call: {
            const auto& callee = syntaxChild(0);
            const NodeId arguments = first + 1; const size_t argumentCount = childCount - 1;
            if (callee.kind == Kind::Identifier && (callee.text == "int" || callee.text == "float" || callee.text == "bool" || callee.text == "string")) {
                for (size_t i{}; i < childCount; ++i) child(i);
                if (argumentCount != 1) AddDiagnostic("Cast to '" + callee.text + "' expects exactly one argument", expression.span);
                else if (const auto cast = BuiltinCastLowering(callee.text, Name(Node(arguments).type))) { type = types_.Intern(callee.text); lowering = *cast; }
                else AddDiagnostic("Cannot convert " + DisplayType(Node(arguments).type) + " to '" + callee.text + "'", syntaxChild(1).span);
                break;
            }
            type = child(0);
            for (size_t i = 1; i < childCount; ++i) child(i);
            if (callee.kind == Kind::Identifier) if (const auto* builtin = FindBuiltinFunction(callee.text)) {
                CheckBuiltinArguments(builtin->name, builtin->parameters, builtin->variadic, expression, arguments, argumentCount);
                type = types_.Intern(builtin->returnType); lowering = builtin->lowering; break;
            }
            if (types_.IsFunction(type)) {
                const auto expected = types_.Arity(type);
                if (argumentCount != expected) AddDiagnostic("Expected " + std::to_string(expected) + " argument(s) for lambda, got " + std::to_string(argumentCount), expression.span);
                type = types_.Element(type); break;
            }
            ResolveCall(expression, callee, first, arguments, argumentCount, owner, type, symbol, lowering, nativeMember);
            break;
        }
        case Kind::Index: {
            const auto target = child(0), index = child(1);
            if (types_.IsList(target)) { type = types_.Element(target); lowering = Lowering::ListIndex; if (index != TypeTable::Int && index != TypeTable::Any && index != TypeTable::Unknown) AddDiagnostic("List index must be 'int', got " + DisplayType(index), syntaxChild(1).span); }
            else if (IsOpen(target)) type = TypeTable::Any;
            else if (target != TypeTable::Unknown) AddDiagnostic("Type '" + Name(target) + "' is not indexable", expression.span);
            break;
        }
        case Kind::Slice: {
            type = child(0); const auto low = child(1), high = child(2);
            if (!types_.IsList(type)) AddDiagnostic("Only lists can be sliced", expression.span);
            if (low != TypeTable::Int || high != TypeTable::Int) AddDiagnostic("Slice bounds must be 'int'", expression.span); break;
        }
        case Kind::Range: {
            const auto low = child(0), high = child(1); type = TypeTable::Range;
            if (low != TypeTable::Int || high != TypeTable::Int) AddDiagnostic("Range bounds must be 'int'", expression.span);
            break;
        }
        case Kind::Lambda: {
            auto lambdaLocals = locals; const size_t lambdaDepth = lambdaLocals.empty() ? 1 : lambdaLocals.back().depth + 1;
            const auto parameters = Syntax().Parameters(expression);
            for (const auto& parameter : parameters) AddLocal(lambdaLocals, parameter.name, TypeTable::Any, SymbolKind::Parameter, parameter.span, expression.span, lambdaDepth);
            TypeId resultType{};
            ++lambdaNesting_;
            if (expression.lambdaIsBlock) {
                lambdaBody = AnalyzeStatements(expression.lambdaBody, lambdaLocals, owner, InferReturn, expression.span.End(), lambdaDepth + 1);
                resultType = LambdaResult(expression, lambdaBody);
            } else {
                assert(childCount == 1);
                ++visited; resultType = Infer(syntaxChild(0), lambdaLocals, owner, first);
            }
            --lambdaNesting_;
            type = types_.Function(parameters.size(), resultType == TypeTable::Unknown ? TypeTable::Any : resultType); break;
        }
        case Kind::Is: case Kind::As: {
            const auto operand = child(0);
            const auto target = types_.Intern(expression.text); CheckType(target, expression.span);
            type = expression.kind == Kind::Is ? TypeTable::Bool : target;
            CheckTypeTest(expression, operand, target);
            break;
        }
        case Kind::Unary: {
            const auto operand = child(0);
            if (expression.text == "!") { type = TypeTable::Bool; if (operand != TypeTable::Bool && operand != TypeTable::Any) AddDiagnostic("Operator '!' requires 'bool', got " + DisplayType(operand), expression.span); }
            else {
                type = operand;
                if (const auto* native = FindBuiltinUnary(Name(operand), expression.text)) { nativeOperator = native; type = types_.Intern(native->result); }
                else if (!IsNumeric(operand) && operand != TypeTable::Any) AddDiagnostic("Unary '" + expression.text + "' is not defined for " + DisplayType(operand), expression.span);
            }
            break;
        }
        case Kind::Binary: {
            const auto left = child(0), right = child(1); const auto& operation = expression.text;
            if (operation == "&&" || operation == "||") { type = TypeTable::Bool; if ((left != TypeTable::Bool && left != TypeTable::Any) || (right != TypeTable::Bool && right != TypeTable::Any)) AddDiagnostic("Logical operator requires 'bool' operands", expression.span); }
            else if ((operation == "==" || operation == "!=") && (left == TypeTable::Null || right == TypeTable::Null)) {
                const auto other = left == TypeTable::Null ? right : left; type = TypeTable::Bool;
                if (IsValueType(other)) AddDiagnostic("Cannot compare 'null' with '" + Name(other) + "'", expression.span);
            }
            else if (left == TypeTable::Any || right == TypeTable::Any) type = operation == "==" || operation == "!=" || operation == "<" || operation == ">" || operation == "<=" || operation == ">=" ? TypeTable::Bool : TypeTable::Any;
            else if (const auto* builtin = FindBuiltinOperator(Name(left), operation, Name(right))) { type = types_.Intern(builtin->result); lowering = builtin->lowering; if (!builtin->cName.empty()) nativeOperator = builtin; }
            else if (left != TypeTable::Unknown && right != TypeTable::Unknown) AddDiagnostic("Operator '" + operation + "' is not defined for '" + Name(left) + "' and '" + Name(right) + "'", expression.span);
            break;
        }
        case Kind::Assignment: {
            const auto left = child(0), right = child(1); type = left;
            const auto targetKind = syntaxChild(0).kind;
            if (targetKind != Kind::Identifier && targetKind != Kind::Member && targetKind != Kind::Index) AddDiagnostic("Invalid assignment target", syntaxChild(0).span);
            if (targetKind == Kind::Index && types_.IsList(ChildType(first, 0))) lowering = Lowering::ListSet;
            if (targetKind == Kind::Member && IsStructType(ChildType(first, 0)) && !IsAddressable(Node(first).children.first)) AddDiagnostic("Cannot modify a member of a temporary struct value", syntaxChild(0).span);
            if (expression.text == "=") {
                if (!Accepts(left, first + 1)) AddDiagnostic("Cannot assign " + DisplayType(right) + " to " + DisplayType(left), syntaxChild(1).span);
            } else if (left == TypeTable::Any || right == TypeTable::Any) {
                // Checked at run time; a string operand makes "+=" a concatenation.
                const auto other = left == TypeTable::Any ? right : left;
                if (other == TypeTable::String && expression.text == "+=") { if (lowering != Lowering::ListSet) lowering = Lowering::CompoundConcat; }
                else if (left == TypeTable::Int && expression.text == "/=") AddDiagnostic("Result of '/=' is 'float', which cannot be assigned to 'int'", expression.span);
                else if (other != TypeTable::Any && other != TypeTable::Unknown && !IsNumeric(other)) AddDiagnostic("Operator '" + expression.text + "' is not defined for '" + Name(left) + "' and '" + Name(right) + "'", expression.span);
            } else if (const auto* builtin = FindBuiltinOperator(Name(left), std::string_view(expression.text).substr(0, 1), Name(right))) {
                if (!Assignable(left, types_.Intern(builtin->result))) AddDiagnostic("Result of '" + expression.text + "' is '" + builtin->result + "', which cannot be assigned to '" + Name(left) + "'", expression.span);
                if (!builtin->cName.empty()) nativeOperator = builtin;
                if (builtin->lowering == Lowering::Concat && lowering != Lowering::ListSet) lowering = Lowering::CompoundConcat;
            } else if (left != TypeTable::Unknown && right != TypeTable::Unknown) AddDiagnostic("Operator '" + expression.text + "' is not defined for '" + Name(left) + "' and '" + Name(right) + "'", expression.span);
            break;
        }
        }
        auto& bound = Node(slot);
        bound.kind = expression.kind; bound.lowering = lowering; bound.type = type; bound.span = expression.span; bound.symbolIndex = symbol;
        bound.children = {first, visited}; bound.lambdaBody = lambdaBody; bound.syntax = &expression;
        bound.nativeMember = nativeMember; bound.nativeOperator = nativeOperator;
        return type;
    }
    // A class or an external reference type: not a struct, enum or builtin.
    bool IsClassType(TypeId type) const {
        return types_.Kind(type) == TypeKind::Named && declaredTypes_.contains(Name(type)) && !IsStructType(type) && !enumTypes_.contains(Name(type));
    }
    bool IsReferenceType(TypeId type) const { return IsClassType(type) || type == TypeTable::String || type == TypeTable::Table || types_.IsList(type); }
    // `x is T` and `x as T`: x is a class value (T a related class) or `any` (T any type a value can hold).
    void CheckTypeTest(const SyntaxExpression& expression, TypeId operand, TypeId target) {
        if (operand == TypeTable::Unknown || target == TypeTable::Unknown) return;
        const auto keyword = std::string(expression.kind == SyntaxExpression::Kind::Is ? "'is'" : "'as'");
        if (operand == TypeTable::Any) {
            const bool holdable = target == TypeTable::Int || target == TypeTable::Float || target == TypeTable::Bool || enumTypes_.contains(Name(target)) || IsStructType(target) || IsReferenceType(target);
            if (!holdable) AddDiagnostic(keyword + " cannot test for type '" + Name(target) + "'", expression.span);
            else if (expression.kind == SyntaxExpression::Kind::As && !IsReferenceType(target)) AddDiagnostic("'as' requires a reference type, got '" + Name(target) + "'; use 'is' and a conversion", expression.span);
            return;
        }
        if (!IsClassType(operand)) { AddDiagnostic(keyword + " requires a class or 'any' operand, got " + DisplayType(operand), expression.span); return; }
        if (!IsClassType(target)) AddDiagnostic("A '" + Name(operand) + "' value can never be '" + Name(target) + "'", expression.span);
        else if (!Assignable(target, operand) && !Assignable(operand, target)) AddDiagnostic("A '" + Name(operand) + "' value can never be '" + Name(target) + "'; the classes are not related", expression.span);
    }
    TypeId LambdaResult(const SyntaxExpression& expression, NodeRange body) {
        std::vector<TypeId> returns; bool bareReturn = false;
        const auto collect = [&](auto&& self, NodeRange statements) -> void {
            for (NodeId i{}; i < statements.count; ++i) {
                const auto statement = result_.bound.statements[statements.first + i];
                if (statement.kind == SyntaxStatement::Kind::Return) {
                    const auto& value = Node(statement.expression);
                    if (value.kind == SyntaxExpression::Kind::Missing) bareReturn = true;
                    else returns.push_back(value.type);
                }
                self(self, statement.body); self(self, statement.elseBody);
            }
        };
        collect(collect, body);
        if (returns.empty()) return TypeTable::Void;
        auto result = returns.front();
        for (size_t i = 1; i < returns.size(); ++i) {
            if (result == returns[i]) continue;
            if ((result == TypeTable::Int && returns[i] == TypeTable::Float) || (result == TypeTable::Float && returns[i] == TypeTable::Int)) result = TypeTable::Float;
            else if (result == TypeTable::Any || returns[i] == TypeTable::Any) result = TypeTable::Any;
            else if (Assignable(result, returns[i])) continue;
            else if (Assignable(returns[i], result)) result = returns[i];
            else { AddDiagnostic("Lambda return types are inconsistent", expression.span); result = TypeTable::Any; break; }
        }
        if (bareReturn) AddDiagnostic("Lambda cannot mix value and valueless returns", expression.span);
        if (!AlwaysReturns(expression.lambdaBody)) AddDiagnostic("Not all lambda code paths return a value", expression.span);
        return result;
    }
    std::string DisplayType(TypeId type) const {
        if (types_.IsFunction(type)) return "'lambda with " + std::to_string(types_.Arity(type)) + " parameter(s) returning " + Name(types_.Element(type)) + "'";
        return type == TypeTable::Unknown ? "unknown value" : "'" + Name(type) + "'";
    }
    // A table literal in a list context becomes a list literal, nested literals included.
    void CoerceLiteral(NodeId slot, TypeId expected) {
        auto& node = Node(slot);
        if (node.kind != SyntaxExpression::Kind::Table || !types_.IsList(expected)) return;
        node.type = expected;
        const auto element = types_.Element(expected);
        for (NodeId i{}; i < node.children.count; ++i) CoerceLiteral(node.children.first + i, element);
    }
    // Checks the value bound at slot against expected, after list-literal coercion.
    bool Accepts(TypeId expected, NodeId slot) {
        CoerceLiteral(slot, expected);
        return Assignable(expected, Node(slot).type);
    }
    bool Assignable(TypeId expected, TypeId actual) const {
        if (expected == TypeTable::Unknown || actual == TypeTable::Unknown || expected == TypeTable::Any || actual == TypeTable::Any || expected == actual) return true;
        if (actual == TypeTable::Null) return expected == TypeTable::Table || types_.IsList(expected) || (!IsBuiltinType(Name(expected)) && !enumTypes_.contains(Name(expected)) && !IsStructType(expected) && expected != TypeTable::Void);
        if (types_.IsList(expected) && actual == TypeTable::Table) return true;
        if (expected == TypeTable::Float && actual == TypeTable::Int) return true;
        if (expected == TypeTable::Table && types_.IsList(actual)) return true;
        const std::string* current = &Name(actual);
        for (size_t depth{}; depth < result_.baseTypes.size() + 1; ++depth) {
            const auto base = result_.baseTypes.find(*current);
            if (base == result_.baseTypes.end()) break;
            if (base->second == Name(expected)) return true;
            current = &base->second;
        }
        return false;
    }
    size_t BuiltinSymbolIndex(const BuiltinFunction& builtin) {
        if (const auto existing = builtinSymbols_.find(&builtin); existing != builtinSymbols_.end()) return existing->second;
        std::vector<std::string> parameters; for (const auto& parameter : builtin.parameters) parameters.push_back(parameter.type + " " + parameter.name);
        if (builtin.variadic) parameters.push_back("any values...");
        const auto index = AddSymbol(builtin.name, builtin.returnType, SymbolKind::Function, {source_.size(), 0}, std::move(parameters));
        builtinSymbols_.emplace(&builtin, index);
        return index;
    }
    TextSpan MemberNameSpan(const SyntaxExpression& expression) const { return {expression.span.End() - expression.text.size(), expression.text.size()}; }
    void AddLocal(std::vector<Local>& locals, std::string_view name, TypeId type, SymbolKind kind, TextSpan declaration, TextSpan visibility, size_t depth) {
        const auto duplicate = std::find_if(locals.rbegin(), locals.rend(), [&](const Local& local) { return local.depth == depth && local.name == name; });
        if (duplicate != locals.rend()) AddDiagnostic("Variable '" + std::string(name) + "' is already declared in this scope", declaration);
        const auto index = AddSymbol(std::string(name), Name(type), kind, declaration); locals.push_back({name, type, index, depth});
        result_.bindings.push_back({name, type, declaration, visibility, depth, index});
        if (const auto found = result_.variableTypes.find(name); found != result_.variableTypes.end()) found->second = type;
        else result_.variableTypes.emplace(std::string(name), type);
    }
    NodeRange AnalyzeStatements(NodeRange range, std::vector<Local>& locals, const SyntaxClass* owner, TypeId returnType, size_t scopeEnd, size_t depth, size_t loopDepth = 0, bool preserveLocals = false) {
        using StatementKind = SyntaxStatement::Kind;
        const auto statements = Syntax().Statements(range);
        const auto first = static_cast<NodeId>(result_.bound.statements.size());
        result_.bound.statements.resize(first + statements.size());
        const size_t startingLocals = locals.size();
        bool unreachable = false;
        for (size_t at{}; at < statements.size(); ++at) {
            const auto& statement = statements[at];
            const auto& expression = Syntax().Expression(statement.expression);
            if (unreachable) AddDiagnostic("Unreachable code", statement.span, DiagnosticSeverity::Warning);
            BoundStatement bound; bound.kind = statement.kind; bound.inferredType = statement.inferredType; bound.span = statement.span; bound.syntax = &statement;
            const auto infer = [&](const SyntaxExpression& value, NodeId& slot) { slot = AllocateExpressions(1); return Infer(value, locals, owner, slot); };
            if (statement.kind == StatementKind::Local) {
                const auto actual = infer(expression, bound.expression); const auto type = statement.inferredType ? actual : types_.Intern(statement.type); bound.type = type;
                const TextSpan nameSpan = statement.nameSpan;
                if (!statement.inferredType) {
                    CheckType(type, nameSpan); if (expression.kind != SyntaxExpression::Kind::Missing && !Accepts(type, bound.expression)) AddDiagnostic("Cannot assign " + DisplayType(actual) + " to '" + Name(type) + "'", expression.span);
                    if (types_.IsList(type) && expression.kind == SyntaxExpression::Kind::Table) {
                        const auto element = types_.Element(type); const auto entries = Node(bound.expression).children;
                        for (NodeId i{}; i < entries.count; ++i) {
                            const auto& entry = Node(entries.first + i);
                            if (entry.kind == SyntaxExpression::Kind::TableEntry) AddDiagnostic("Typed list literals cannot contain keyed entries", Syntax().Child(expression, i).span);
                            else if (!Assignable(element, entry.type)) AddDiagnostic("List element " + std::to_string(i + 1) + " expects '" + Name(element) + "', got " + DisplayType(entry.type), Syntax().Child(expression, i).span);
                        }
                    }
                }
                if (statement.inferredType && type == TypeTable::Unknown) AddDiagnostic("Cannot infer type of '" + statement.name + "'", nameSpan);
                if (statement.inferredType && type == TypeTable::Void) AddDiagnostic("Cannot initialize '" + statement.name + "' with a void expression", expression.span);
                AddLocal(locals, statement.name, type, SymbolKind::Variable, nameSpan, {statement.span.offset, scopeEnd - statement.span.offset}, depth);
            } else if (statement.kind == StatementKind::Return) {
                const auto actual = infer(expression, bound.expression);
                const bool hasValue = expression.kind != SyntaxExpression::Kind::Missing;
                if (returnType == InferReturn) { }
                else if (returnType == TypeTable::Void && hasValue) AddDiagnostic("Void function cannot return a value", expression.span);
                else if (returnType != TypeTable::Void && !hasValue) AddDiagnostic("Function returning '" + Name(returnType) + "' must return a value", statement.span);
                else if (returnType != TypeTable::Void && !Accepts(returnType, bound.expression)) AddDiagnostic("Cannot return " + DisplayType(actual) + " from function returning '" + Name(returnType) + "'", expression.span);
            } else if (statement.kind == StatementKind::Break || statement.kind == StatementKind::Continue) {
                if (loopDepth == 0) AddDiagnostic(std::string(statement.kind == StatementKind::Break ? "'break'" : "'continue'") + " can only be used inside a loop", statement.span);
            } else if (statement.kind == StatementKind::For) {
                const size_t loopLocals = locals.size();
                bound.initializer = AnalyzeStatements(statement.initializer, locals, owner, returnType, statement.span.End(), depth + 1, loopDepth, true);
                const auto condition = infer(expression, bound.expression);
                if (condition != TypeTable::Bool && condition != TypeTable::Any && condition != TypeTable::Unknown) AddDiagnostic("For condition must be 'bool', got " + DisplayType(condition), expression.span);
                infer(Syntax().Expression(statement.increment), bound.increment);
                bound.body = AnalyzeStatements(statement.body, locals, owner, returnType, statement.span.End(), depth + 2, loopDepth + 1);
                locals.resize(loopLocals);
            } else if (statement.kind == StatementKind::ForEach) {
                const size_t loopLocals = locals.size(); const auto iterable = infer(expression, bound.expression);
                TypeId firstType{};
                if (iterable == TypeTable::Range) {
                    firstType = TypeTable::Int;
                    if (!statement.secondName.empty()) AddDiagnostic("Range loops require exactly one iteration variable", statement.secondNameSpan);
                } else if (types_.IsList(iterable)) {
                    firstType = statement.secondName.empty() ? types_.Element(iterable) : TypeTable::Int;
                } else if (iterable == TypeTable::Table) {
                    firstType = TypeTable::Any;
                    if (statement.secondName.empty()) AddDiagnostic("Table loops require key and value iteration variables", statement.nameSpan);
                } else if (iterable == TypeTable::Any) {
                    firstType = TypeTable::Any;
                } else if (iterable != TypeTable::Unknown) AddDiagnostic("Type '" + Name(iterable) + "' is not iterable", expression.span);
                AddLocal(locals, statement.name, firstType, SymbolKind::Variable, statement.nameSpan, {statement.nameSpan.offset, statement.span.End() - statement.nameSpan.offset}, depth + 1);
                if (!statement.secondName.empty()) {
                    const auto secondType = types_.IsList(iterable) ? types_.Element(iterable) : TypeTable::Any;
                    AddLocal(locals, statement.secondName, secondType, SymbolKind::Variable, statement.secondNameSpan, {statement.secondNameSpan.offset, statement.span.End() - statement.secondNameSpan.offset}, depth + 1);
                }
                bound.body = AnalyzeStatements(statement.body, locals, owner, returnType, statement.span.End(), depth + 2, loopDepth + 1);
                locals.resize(loopLocals);
            } else {
                const auto condition = infer(expression, bound.expression);
                if (statement.kind == StatementKind::If || statement.kind == StatementKind::While) {
                    if (condition != TypeTable::Bool && condition != TypeTable::Any && condition != TypeTable::Unknown) AddDiagnostic(std::string(statement.kind == StatementKind::If ? "If" : "While") + " condition must be 'bool', got " + DisplayType(condition), expression.span);
                    bound.body = AnalyzeStatements(statement.body, locals, owner, returnType, statement.span.End(), depth + 1, loopDepth + (statement.kind == StatementKind::While ? 1 : 0));
                    if (statement.kind == StatementKind::If) bound.elseBody = AnalyzeStatements(statement.elseBody, locals, owner, returnType, statement.span.End(), depth + 1, loopDepth);
                }
            }
            result_.bound.statements[first + at] = bound;
            if (statement.kind == StatementKind::Return || statement.kind == StatementKind::Break || statement.kind == StatementKind::Continue || (statement.kind == StatementKind::If && statement.elseBody.count && AlwaysReturns(statement.body) && AlwaysReturns(statement.elseBody))) unreachable = true;
        }
        if (!preserveLocals) locals.resize(startingLocals);
        return {first, static_cast<NodeId>(statements.size())};
    }
    void AnalyzeBodies() {
        result_.bound.span = result_.syntax.root.span;
        for (const auto& value : result_.syntax.root.enums) {
            BoundEnum boundEnum; boundEnum.name = value.name; boundEnum.span = value.span; long long nextValue{};
            for (const auto& member : value.members) {
                long long resolved = nextValue;
                const auto& initializer = Syntax().Expression(member.initializer);
                if (initializer.kind != SyntaxExpression::Kind::Missing) {
                    const SyntaxExpression* number = &initializer; int sign = 1;
                    if (number->kind == SyntaxExpression::Kind::Unary && (number->text == "-" || number->text == "+") && number->children.count == 1) { sign = number->text == "-" ? -1 : 1; number = &Syntax().Child(*number, 0); }
                    if (number->kind != SyntaxExpression::Kind::Number || number->text.find('.') != std::string::npos) AddDiagnostic("Enum values must be integer constants", initializer.span);
                    else try { resolved = std::stoll(number->text) * sign; } catch (...) { AddDiagnostic("Enum value is outside the supported integer range", initializer.span); }
                }
                boundEnum.members.push_back({member.name, resolved, member.nameSpan}); nextValue = resolved + 1;
            }
            result_.bound.enums.push_back(std::move(boundEnum));
        }
        for (const auto& value : result_.syntax.root.classes) {
            if (value.isExtern) continue; // no code; ExportTypes describes it to the C backend
            const bool hasConstructor = std::any_of(value.functions.begin(), value.functions.end(), [](const auto& function) { return function.isConstructor; });
            if (!hasConstructor) RequireDefaultBaseConstructor(value, value.span);
            BoundClass boundClass; boundClass.name = value.name; boundClass.baseName = value.baseName; boundClass.span = value.span; boundClass.isStruct = value.isStruct;
            boundClass.isAbstract = value.isAbstract; boundClass.isSealed = value.isSealed;
            for (const auto& field : value.fields) {
                BoundField boundField; boundField.access = field.access; boundField.type = field.type; boundField.name = field.name; boundField.span = field.span; boundField.isStatic = field.isStatic;
                const auto& initializer = Syntax().Expression(field.initializer);
                if (initializer.kind != SyntaxExpression::Kind::Missing) {
                    std::vector<Local> none; boundField.initializer = AllocateExpressions(1);
                    staticContext_ = field.isStatic;
                    const auto actual = Infer(initializer, none, &value, boundField.initializer);
                    staticContext_ = false;
                    if (!Accepts(types_.Intern(field.type), boundField.initializer)) AddDiagnostic("Cannot assign " + DisplayType(actual) + " to '" + field.type + "'", initializer.span);
                }
                boundClass.fields.push_back(std::move(boundField));
            }
            for (const auto& function : value.functions) {
                BoundFunction boundFunction; boundFunction.access = function.access; boundFunction.name = function.name; boundFunction.returnType = function.returnType; boundFunction.isConstructor = function.isConstructor; boundFunction.parameters = function.parameters; boundFunction.span = function.span;
                boundFunction.isStatic = function.isStatic; boundFunction.symbolIndex = functionSymbols_.at(&function);
                boundFunction.isVirtual = result_.symbols[boundFunction.symbolIndex].isVirtual; boundFunction.isAbstract = function.modifiers.isAbstract;
                std::vector<Local> locals; const size_t scopeEnd = FunctionEnd(function);
                // Parameters first, so base constructor arguments see them but not `this`.
                std::unordered_set<std::string_view> parameterNames;
                for (const auto& parameter : function.parameters) {
                    const auto nameSpan = ParameterNameSpan(parameter);
                    if (!parameterNames.insert(parameter.name).second) AddDiagnostic("Parameter '" + parameter.name + "' is already declared", nameSpan);
                    AddLocal(locals, parameter.name, types_.Intern(parameter.type), SymbolKind::Parameter, nameSpan, {nameSpan.offset, scopeEnd - nameSpan.offset}, 0);
                }
                if (function.isConstructor) BindBaseCall(value, function, locals, boundFunction);
                staticContext_ = function.isStatic;
                if (!function.isStatic) AddLocal(locals, "this", types_.Intern(value.name), SymbolKind::Parameter, function.span, {function.span.offset, scopeEnd - function.span.offset}, 0);
                boundFunction.body = AnalyzeStatements(function.body, locals, &value, function.isConstructor ? TypeTable::Void : types_.Intern(function.returnType), scopeEnd, 1);
                if (!function.isConstructor && function.returnType != "void" && function.hasBody && !AlwaysReturns(function.body)) AddDiagnostic("Not all code paths return a value from '" + function.name + "'", function.span);
                staticContext_ = false;
                boundClass.functions.push_back(std::move(boundFunction));
            }
            result_.bound.classes.push_back(std::move(boundClass));
        }
        std::vector<Local> locals; result_.bound.topLevel = AnalyzeStatements(result_.syntax.root.topLevel, locals, nullptr, TypeTable::Void, source_.size(), 0);
    }
    // The base class has a constructor that takes no arguments (declared or implicit). External bases are not checked.
    void RequireDefaultBaseConstructor(const SyntaxClass& value, TextSpan span) {
        if (value.isStruct || value.baseName.empty() || !FindClass(value.baseName)) return;
        const auto candidates = ConstructorCandidates(value.baseName);
        if (std::none_of(candidates.begin(), candidates.end(), [](const Callable& candidate) { return candidate.required == 0; }))
            AddDiagnostic("Base class '" + value.baseName + "' has no parameterless constructor; call one with ': base(...)'", span);
    }
    // `: base(args)` binds in the constructor's parameter scope. `this` and instance members are not available yet.
    void BindBaseCall(const SyntaxClass& value, const SyntaxFunction& function, std::vector<Local>& locals, BoundFunction& bound) {
        if (!function.hasBaseCall) { RequireDefaultBaseConstructor(value, function.span); return; }
        bound.hasBaseCall = true;
        if (value.isStruct || value.baseName.empty()) { AddDiagnostic("': base(...)' requires a base class", function.baseSpan); return; }
        const auto arguments = Syntax().Expressions(function.baseArguments);
        const NodeId first = AllocateExpressions(arguments.size());
        staticContext_ = true; staticContextName_ = "base constructor arguments";
        for (size_t i{}; i < arguments.size(); ++i) Infer(arguments[i], locals, &value, first + static_cast<NodeId>(i));
        staticContext_ = false; staticContextName_ = DefaultStaticContext;
        bound.baseArguments = {first, static_cast<NodeId>(arguments.size())};
        if (!FindClass(value.baseName)) return;
        const auto candidates = ConstructorCandidates(value.baseName);
        if (candidates.empty()) return;
        const int pick = ResolveOverload(value.baseName, candidates, first, arguments.size(), function.baseSpan, [&](size_t index) { return arguments[index].span; }, true);
        if (pick < 0) return;
        const auto& chosen = candidates[static_cast<size_t>(pick)];
        bound.baseConstructor = chosen.symbol;
        CoerceArguments(chosen, first, arguments.size());
    }
    size_t FunctionEnd(const SyntaxFunction& function) const {
        size_t end = function.span.End();
        const auto visit = [&](auto&& self, NodeRange statements) -> void { for (const auto& statement : Syntax().Statements(statements)) { end = (std::max)(end, statement.span.End()); self(self, statement.body); self(self, statement.elseBody); } };
        visit(visit, function.body); return end;
    }
    bool AlwaysReturns(NodeRange statements) const {
        for (const auto& statement : Syntax().Statements(statements)) {
            if (statement.kind == SyntaxStatement::Kind::Return) return true;
            if (statement.kind == SyntaxStatement::Kind::If && statement.elseBody.count && AlwaysReturns(statement.body) && AlwaysReturns(statement.elseBody)) return true;
        }
        return false;
    }

    std::string_view source_;
    std::vector<size_t> lineStarts_;
    Analysis result_;
    TypeTable& types_;
    std::vector<TypeId> symbolTypes_; // parallel to result_.symbols
    std::unordered_map<size_t, const BuiltinMember*> symbolBuiltins_; // symbol index -> native member behind it
    std::unordered_map<const SyntaxFunction*, size_t> functionSymbols_;
    StringMap<std::vector<size_t>> constructors_; // type -> constructor symbols, declaration order
    StringSet structTypes_;
    static constexpr std::string_view DefaultStaticContext = "a static method";
    bool staticContext_{};
    std::string_view staticContextName_ = DefaultStaticContext; // where `this` is unavailable, for messages
    bool baseAllowed_{};
    size_t lambdaNesting_{};
    std::unordered_map<std::string_view, const SyntaxClass*> classes_; // first declaration wins
    std::unordered_map<const BuiltinFunction*, size_t> builtinSymbols_;
    StringSet knownTypes_;
    StringSet declaredTypes_;
    StringSet registeredBuiltins_;
    StringSet enumTypes_;
    StringSet nativeTypes_; // extern classes, from this source or from external types
};

} // namespace

Analysis Analyze(std::string_view source) { auto syntax = ParseTree(source); return Analyzer(source, std::move(syntax)).Run(); }
Analysis Analyze(std::string_view source, const std::vector<ExternalType>& externalTypes) { auto syntax = ParseTree(source); return Analyzer(source, std::move(syntax), externalTypes).Run(); }

std::vector<ExternalType> ExportTypes(const Analysis& analysis) {
    std::vector<ExternalType> result;
    for (const auto& symbol : analysis.symbols) {
        if ((symbol.kind != SymbolKind::Class && symbol.kind != SymbolKind::Enum) || !symbol.span.length) continue; // declared in this source
        ExternalType type{symbol.name};
        type.isEnum = symbol.kind == SymbolKind::Enum; type.isStruct = symbol.isStruct; type.isNative = symbol.isNative;
        if (const auto base = analysis.baseTypes.find(symbol.name); base != analysis.baseTypes.end()) type.baseType = base->second;
        if (const auto members = analysis.members.find(type.isEnum ? "$type:" + symbol.name : symbol.name); members != analysis.members.end()) for (const auto index : members->second) type.members.push_back(analysis.symbols[index]);
        if (type.isEnum) {
            const auto bound = std::ranges::find(analysis.bound.enums, symbol.name, &BoundEnum::name);
            assert(bound != analysis.bound.enums.end());
            for (const auto& member : type.members) {
                const auto value = std::ranges::find(bound->members, member.name, &BoundEnumMember::name);
                assert(value != bound->members.end());
                type.values.push_back(value->value);
            }
        } else {
            if (const auto statics = analysis.members.find("$type:" + symbol.name); statics != analysis.members.end()) for (const auto index : statics->second) type.members.push_back(analysis.symbols[index]);
            for (const auto& candidate : analysis.symbols) if (candidate.kind == SymbolKind::Method && candidate.name == symbol.name && candidate.owner == symbol.name && candidate.span.length) type.members.push_back(candidate); // constructors
        }
        result.push_back(std::move(type));
    }
    return result;
}

std::optional<std::string> ResolveVariableType(const Analysis& analysis, std::string_view name, size_t position) {
    const VariableBinding* best = nullptr;
    for (const auto& binding : analysis.bindings) {
        if (binding.name != name || position < binding.declaration.offset || !binding.visibility.Contains(position)) continue;
        if (!best || binding.depth > best->depth || (binding.depth == best->depth && binding.declaration.offset > best->declaration.offset)) best = &binding;
    }
    if (best) return analysis.types.Name(best->type);
    const auto legacy = analysis.variableTypes.find(name);
    return legacy == analysis.variableTypes.end() ? std::nullopt : std::optional<std::string>(analysis.types.Name(legacy->second));
}

const BoundExpression* FindExpression(const Analysis& analysis, size_t position) {
    const BoundExpression* best = nullptr;
    // Children follow their parent in the pool, so <= prefers the innermost of equal spans.
    for (const auto& expression : analysis.bound.expressions) if (expression.syntax && expression.span.Contains(position) && (!best || expression.span.length <= best->span.length)) best = &expression;
    return best;
}

const Symbol* FindMember(const Analysis& analysis, std::string_view type, std::string_view name) {
    const auto members = analysis.members.find(type);
    if (members == analysis.members.end()) return nullptr;
    const auto found = std::find_if(members->second.begin(), members->second.end(), [&](size_t index) { return analysis.symbols[index].name == name; });
    return found == members->second.end() ? nullptr : &analysis.symbols[*found];
}

} // namespace luasharp
