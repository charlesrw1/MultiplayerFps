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
        vec3_ = types_.Intern("Vec3");
        for (const auto& type : BuiltinTypes()) knownTypes_.insert(type);
        RegisterBuiltinType("string"); RegisterBuiltinType("Vec3"); RegisterBuiltinType("Quat"); RegisterBuiltinType("Transform");
        for (const auto& external : externalTypes) {
            knownTypes_.insert(external.name); declaredTypes_.insert(external.name);
            if (external.isEnum) enumTypes_.insert(external.name);
            PushSymbol({external.name, external.name, external.isEnum ? SymbolKind::Enum : SymbolKind::Class, 0, 0, {source_.size(), 0}, {}, external.name, "public"});
            if (!external.baseType.empty()) result_.baseTypes[external.name] = external.baseType;
            auto& members = result_.members[external.isEnum ? "$type:" + external.name : external.name];
            for (auto member : external.members) { member.span = {source_.size(), 0}; member.implicit = true; members.push_back(PushSymbol(std::move(member))); }
        }
    }

    Analysis Run() {
        for (const auto& diagnostic : result_.syntax.diagnostics) AddDiagnostic(diagnostic.message, diagnostic.span);
        DeclareTypes();
        DeclareMembers();
        FlattenInheritance();
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
            symbol.implicit = true;
            const auto index = PushSymbol(std::move(symbol));
            result_.members[key].push_back(index);
        };
        for (const auto& member : BuiltinMembers(type, false)) add(member, type);
        for (const auto& member : BuiltinMembers(type, true)) add(member, "$type:" + type);
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
            if (!declaredTypes_.insert(value.name).second) AddDiagnostic("Type '" + value.name + "' is already declared", value.span);
            knownTypes_.insert(value.name); classes_.try_emplace(value.name, &value);
            const auto index = AddSymbol(value.name, value.name, SymbolKind::Class, value.span); result_.symbols[index].owner = value.name;
            if (!value.baseName.empty()) result_.baseTypes[value.name] = value.baseName;
        }
        for (const auto& value : result_.syntax.root.classes) if (!value.baseName.empty()) CheckType(types_.Intern(value.baseName), value.span);
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
            std::unordered_set<std::string_view> names; bool hasConstructor = false;
            auto& members = result_.members[value.name];
            for (const auto& field : value.fields) {
                CheckType(types_.Intern(field.type), field.span);
                if (!names.insert(field.name).second) AddDiagnostic("Member '" + field.name + "' is already declared on " + value.name, field.span);
                const auto index = AddSymbol(field.name, field.type, SymbolKind::Field, field.span);
                result_.symbols[index].owner = value.name; result_.symbols[index].access = field.access;
                members.push_back(index);
            }
            for (const auto& function : value.functions) {
                if (function.isConstructor && function.name != value.name) AddDiagnostic("Constructor name must match class '" + value.name + "'", function.span);
                if (function.isConstructor && std::exchange(hasConstructor, true)) AddDiagnostic("Class '" + value.name + "' has more than one constructor", function.span);
                if (!function.isConstructor) {
                    CheckType(types_.Intern(function.returnType), function.span);
                    if (!names.insert(function.name).second) AddDiagnostic("Member '" + function.name + "' is already declared on " + value.name, function.span);
                }
                std::vector<std::string> parameters;
                for (const auto& parameter : function.parameters) { CheckType(types_.Intern(parameter.type), ParameterNameSpan(parameter)); parameters.push_back(parameter.type + " " + parameter.name); }
                const auto index = AddSymbol(function.name, function.isConstructor ? value.name : function.returnType, SymbolKind::Method, function.span, std::move(parameters));
                result_.symbols[index].owner = value.name; result_.symbols[index].access = function.access;
                if (!function.isConstructor) members.push_back(index);
            }
        }
    }
    void FlattenInheritance() {
        for (const auto& [derived, firstBase] : result_.baseTypes) {
            std::unordered_set<std::string_view> visited{derived}; std::string_view base = firstBase;
            auto& derivedMembers = result_.members[derived];
            while (!base.empty()) {
                if (!visited.insert(base).second) { AddDiagnostic("Inheritance cycle involving '" + derived + "'", ClassSpan(derived)); break; }
                if (const auto found = result_.members.find(base); found != result_.members.end()) {
                    for (const auto memberIndex : found->second) {
                        const auto& member = result_.symbols[memberIndex];
                        const auto own = std::find_if(derivedMembers.begin(), derivedMembers.end(), [&](size_t index) { return result_.symbols[index].name == member.name; });
                        if (own == derivedMembers.end()) { derivedMembers.push_back(memberIndex); continue; }
                        const auto& mine = result_.symbols[*own];
                        if (member.access != "private" && (mine.kind != member.kind || mine.type != member.type || mine.parameters != member.parameters)) AddDiagnostic("Override of '" + member.name + "' on '" + derived + "' does not match the inherited signature", mine.span);
                    }
                }
                const auto parent = result_.baseTypes.find(base); base = parent == result_.baseTypes.end() ? std::string_view{} : std::string_view(parent->second);
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
    std::optional<size_t> FindMember(std::string_view type, std::string_view name) const {
        auto found = result_.members.find(type);
        if (found == result_.members.end()) {
            const auto base = type.substr(0, type.find("[]"));
            found = result_.members.find(base);
        }
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
    // Arguments of a call are its children after the callee.
    void CheckArguments(const Symbol& callable, const SyntaxExpression& call, NodeId arguments, size_t count) {
        if (count != callable.parameters.size()) {
            AddDiagnostic("Expected " + std::to_string(callable.parameters.size()) + " argument(s) for '" + callable.name + "', got " + std::to_string(count), call.span);
            return;
        }
        for (size_t i{}; i < count; ++i) {
            const std::string_view label = callable.parameters[i];
            const auto expected = types_.Intern(label.substr(0, label.find(' '))), actual = Node(arguments + static_cast<NodeId>(i)).type;
            if (!Assignable(expected, actual)) AddDiagnostic("Argument " + std::to_string(i + 1) + " of '" + callable.name + "' expects '" + Name(expected) + "', got " + DisplayType(actual), Syntax().Child(call, i + 1).span);
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
            if (!Assignable(types_.Intern(parameters[i].type), actual)) AddDiagnostic("Argument " + std::to_string(i + 1) + " of '" + std::string(name) + "' expects '" + parameters[i].type + "', got " + DisplayType(actual), Syntax().Child(call, i + 1).span);
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
            if (const auto* classNode = FindClass(expression.text)) {
                const auto constructor = std::find_if(classNode->functions.begin(), classNode->functions.end(), [](const auto& value) { return value.isConstructor; });
                const size_t expected = constructor == classNode->functions.end() ? 0 : constructor->parameters.size();
                if (childCount != expected) AddDiagnostic("Expected " + std::to_string(expected) + " constructor argument(s) for '" + expression.text + "', got " + std::to_string(childCount), expression.span);
                else for (size_t i{}; i < expected; ++i) {
                    const auto actual = Node(first + static_cast<NodeId>(i)).type;
                    if (!Assignable(types_.Intern(constructor->parameters[i].type), actual)) AddDiagnostic("Constructor argument " + std::to_string(i + 1) + " for '" + expression.text + "' expects '" + constructor->parameters[i].type + "', got " + DisplayType(actual), syntaxChild(i).span);
                }
            }
            break;
        }
        case Kind::Identifier: {
            if (const auto* local = FindLocal(locals, expression.text)) { type = local->type; symbol = local->symbolIndex; }
            else if (owner) {
                if (const auto index = FindMember(owner->name, expression.text)) {
                    const auto& member = result_.symbols[*index];
                    type = symbolTypes_[*index]; symbol = index; lowering = member.kind == SymbolKind::Method ? Lowering::ImplicitMethod : Lowering::ImplicitField;
                    if (member.access == "private" && member.owner != owner->name) AddDiagnostic("Member '" + member.name + "' is private on '" + member.owner + "'", expression.span);
                }
            }
            if (type == TypeTable::Unknown && (IsBuiltinType(expression.text) || enumTypes_.contains(expression.text))) type = types_.Meta(expression.text);
            if (type == TypeTable::Unknown) if (const auto* builtin = FindBuiltinFunction(expression.text)) { type = types_.Builtin(builtin->name); symbol = BuiltinSymbolIndex(*builtin); }
            if (type == TypeTable::Unknown) AddDiagnostic("Unknown identifier '" + expression.text + "'", expression.span);
            break;
        }
        case Kind::Parenthesized: type = child(0); break;
        case Kind::Member: {
            const auto target = child(0);
            if (const auto index = FindMember(Name(target), expression.text)) {
                const auto& member = result_.symbols[*index];
                type = symbolTypes_[*index]; symbol = index;
                if (member.access == "private" && (!owner || member.owner != owner->name)) AddDiagnostic("Member '" + member.name + "' is private on '" + member.owner + "'", MemberNameSpan(expression));
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
            std::optional<size_t> callable;
            const BuiltinMember* builtin = nullptr;
            if (callee.kind == Kind::Member) {
                const auto receiver = ChildType(first, 0); const bool isStatic = types_.Kind(receiver) == TypeKind::Meta;
                builtin = FindBuiltinMember(isStatic ? types_.Target(receiver) : std::string_view(Name(receiver)), callee.text, isStatic); callable = FindMember(Name(receiver), callee.text);
            }
            else if (callee.kind == Kind::Identifier && owner) callable = FindMember(owner->name, callee.text);
            if (builtin) { CheckBuiltinArguments(builtin->name, builtin->parameters, false, expression, arguments, argumentCount); lowering = builtin->lowering == Lowering::None && builtin->isStatic ? Lowering::StaticCall : builtin->lowering; }
            else if (callable && result_.symbols[*callable].kind == SymbolKind::Method) CheckArguments(result_.symbols[*callable], expression, arguments, argumentCount);
            else if (type != TypeTable::Unknown && type != TypeTable::Any) AddDiagnostic("Expression of type " + DisplayType(type) + " is not callable", callee.span);
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
            if (expression.lambdaIsBlock) {
                lambdaBody = AnalyzeStatements(expression.lambdaBody, lambdaLocals, owner, InferReturn, expression.span.End(), lambdaDepth + 1);
                resultType = LambdaResult(expression, lambdaBody);
            } else {
                assert(childCount == 1);
                ++visited; resultType = Infer(syntaxChild(0), lambdaLocals, owner, first);
            }
            type = types_.Function(parameters.size(), resultType == TypeTable::Unknown ? TypeTable::Any : resultType); break;
        }
        case Kind::Unary: {
            const auto operand = child(0);
            if (expression.text == "!") { type = TypeTable::Bool; if (operand != TypeTable::Bool && operand != TypeTable::Any) AddDiagnostic("Operator '!' requires 'bool', got " + DisplayType(operand), expression.span); }
            else { type = operand; if (!IsNumeric(operand) && operand != vec3_ && operand != TypeTable::Any) AddDiagnostic("Unary '" + expression.text + "' is not defined for " + DisplayType(operand), expression.span); }
            break;
        }
        case Kind::Binary: {
            const auto left = child(0), right = child(1); const auto& operation = expression.text;
            if (operation == "&&" || operation == "||") { type = TypeTable::Bool; if ((left != TypeTable::Bool && left != TypeTable::Any) || (right != TypeTable::Bool && right != TypeTable::Any)) AddDiagnostic("Logical operator requires 'bool' operands", expression.span); }
            else if ((operation == "==" || operation == "!=") && (left == TypeTable::Null || right == TypeTable::Null)) {
                const auto other = left == TypeTable::Null ? right : left; type = TypeTable::Bool;
                if (other == TypeTable::Int || other == TypeTable::Float || other == TypeTable::Bool) AddDiagnostic("Cannot compare 'null' with '" + Name(other) + "'", expression.span);
            }
            else if (left == TypeTable::Any || right == TypeTable::Any) type = operation == "==" || operation == "!=" || operation == "<" || operation == ">" || operation == "<=" || operation == ">=" ? TypeTable::Bool : TypeTable::Any;
            else if (const auto* builtin = FindBuiltinOperator(Name(left), operation, Name(right))) { type = types_.Intern(builtin->result); lowering = builtin->lowering; }
            else if (left != TypeTable::Unknown && right != TypeTable::Unknown) AddDiagnostic("Operator '" + operation + "' is not defined for '" + Name(left) + "' and '" + Name(right) + "'", expression.span);
            break;
        }
        case Kind::Assignment: {
            const auto left = child(0), right = child(1); type = left;
            const auto targetKind = syntaxChild(0).kind;
            if (targetKind != Kind::Identifier && targetKind != Kind::Member && targetKind != Kind::Index) AddDiagnostic("Invalid assignment target", syntaxChild(0).span);
            if (targetKind == Kind::Index && types_.IsList(ChildType(first, 0))) lowering = Lowering::ListSet;
            if (expression.text == "=") {
                if (!Assignable(left, right)) AddDiagnostic("Cannot assign " + DisplayType(right) + " to " + DisplayType(left), syntaxChild(1).span);
            } else if (const auto* builtin = FindBuiltinOperator(Name(left), std::string_view(expression.text).substr(0, 1), Name(right))) {
                if (!Assignable(left, types_.Intern(builtin->result))) AddDiagnostic("Result of '" + expression.text + "' is '" + builtin->result + "', which cannot be assigned to '" + Name(left) + "'", expression.span);
                if (builtin->lowering == Lowering::Concat) lowering = Lowering::CompoundConcat;
            } else if (left != TypeTable::Unknown && right != TypeTable::Unknown) AddDiagnostic("Operator '" + expression.text + "' is not defined for '" + Name(left) + "' and '" + Name(right) + "'", expression.span);
            break;
        }
        }
        auto& bound = Node(slot);
        bound.kind = expression.kind; bound.lowering = lowering; bound.type = type; bound.span = expression.span; bound.symbolIndex = symbol;
        bound.children = {first, visited}; bound.lambdaBody = lambdaBody; bound.syntax = &expression;
        return type;
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
    bool Assignable(TypeId expected, TypeId actual) const {
        if (expected == TypeTable::Unknown || actual == TypeTable::Unknown || expected == TypeTable::Any || actual == TypeTable::Any || expected == actual) return true;
        if (actual == TypeTable::Null) return expected == TypeTable::Table || types_.IsList(expected) || (!IsBuiltinType(Name(expected)) && !enumTypes_.contains(Name(expected)) && expected != TypeTable::Void);
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
                    CheckType(type, nameSpan); if (expression.kind != SyntaxExpression::Kind::Missing && !Assignable(type, actual)) AddDiagnostic("Cannot assign " + DisplayType(actual) + " to '" + Name(type) + "'", expression.span);
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
                else if (returnType != TypeTable::Void && !Assignable(returnType, actual)) AddDiagnostic("Cannot return " + DisplayType(actual) + " from function returning '" + Name(returnType) + "'", expression.span);
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
            if (!value.baseName.empty()) if (const auto* base = FindClass(value.baseName)) {
                const auto constructor = std::find_if(base->functions.begin(), base->functions.end(), [](const auto& function) { return function.isConstructor; });
                if (constructor != base->functions.end() && !constructor->parameters.empty()) AddDiagnostic("Base class '" + value.baseName + "' requires constructor arguments, but base constructor arguments are not supported", value.span);
            }
            BoundClass boundClass; boundClass.name = value.name; boundClass.baseName = value.baseName; boundClass.span = value.span;
            for (const auto& field : value.fields) {
                BoundField boundField; boundField.access = field.access; boundField.type = field.type; boundField.name = field.name; boundField.span = field.span;
                const auto& initializer = Syntax().Expression(field.initializer);
                if (initializer.kind != SyntaxExpression::Kind::Missing) {
                    std::vector<Local> none; boundField.initializer = AllocateExpressions(1);
                    const auto actual = Infer(initializer, none, &value, boundField.initializer);
                    if (!Assignable(types_.Intern(field.type), actual)) AddDiagnostic("Cannot assign " + DisplayType(actual) + " to '" + field.type + "'", initializer.span);
                }
                boundClass.fields.push_back(std::move(boundField));
            }
            for (const auto& function : value.functions) {
                BoundFunction boundFunction; boundFunction.access = function.access; boundFunction.name = function.name; boundFunction.returnType = function.returnType; boundFunction.isConstructor = function.isConstructor; boundFunction.parameters = function.parameters; boundFunction.span = function.span;
                std::vector<Local> locals; const size_t scopeEnd = FunctionEnd(function);
                AddLocal(locals, "this", types_.Intern(value.name), SymbolKind::Parameter, function.span, {function.span.offset, scopeEnd - function.span.offset}, 0);
                std::unordered_set<std::string_view> parameterNames;
                for (const auto& parameter : function.parameters) {
                    const auto nameSpan = ParameterNameSpan(parameter);
                    if (!parameterNames.insert(parameter.name).second) AddDiagnostic("Parameter '" + parameter.name + "' is already declared", nameSpan);
                    AddLocal(locals, parameter.name, types_.Intern(parameter.type), SymbolKind::Parameter, nameSpan, {nameSpan.offset, scopeEnd - nameSpan.offset}, 0);
                }
                boundFunction.body = AnalyzeStatements(function.body, locals, &value, function.isConstructor ? TypeTable::Void : types_.Intern(function.returnType), scopeEnd, 1);
                if (!function.isConstructor && function.returnType != "void" && !AlwaysReturns(function.body)) AddDiagnostic("Not all code paths return a value from '" + function.name + "'", function.span);
                boundClass.functions.push_back(std::move(boundFunction));
            }
            result_.bound.classes.push_back(std::move(boundClass));
        }
        std::vector<Local> locals; result_.bound.topLevel = AnalyzeStatements(result_.syntax.root.topLevel, locals, nullptr, TypeTable::Void, source_.size(), 0);
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
    TypeId vec3_{};
    std::vector<TypeId> symbolTypes_; // parallel to result_.symbols
    std::unordered_map<std::string_view, const SyntaxClass*> classes_; // first declaration wins
    std::unordered_map<const BuiltinFunction*, size_t> builtinSymbols_;
    StringSet knownTypes_;
    StringSet declaredTypes_;
    StringSet registeredBuiltins_;
    StringSet enumTypes_;
};

} // namespace

Analysis Analyze(std::string_view source) { auto syntax = ParseTree(source); return Analyzer(source, std::move(syntax)).Run(); }
Analysis Analyze(std::string_view source, const std::vector<ExternalType>& externalTypes) { auto syntax = ParseTree(source); return Analyzer(source, std::move(syntax), externalTypes).Run(); }

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
