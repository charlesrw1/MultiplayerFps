#include "Semantic.h"
#include "Builtins.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace luasharp {
namespace {

struct Local {
    std::string name;
    std::string type;
    size_t symbolIndex{};
    size_t depth{};
};

class Analyzer {
public:
    Analyzer(std::string_view source, SyntaxTree syntax, const std::vector<ExternalType>& externalTypes = {}) : source_(source) {
        result_.syntax = std::move(syntax);
        for (const auto& type : BuiltinTypes()) knownTypes_.insert(type);
        RegisterBuiltinType("string"); RegisterBuiltinType("Vec3"); RegisterBuiltinType("Quat"); RegisterBuiltinType("Transform");
        for (const auto& external : externalTypes) {
            knownTypes_.insert(external.name); declaredTypes_.insert(external.name);
            if (external.isEnum) enumTypes_.insert(external.name);
            result_.symbols.push_back({external.name, external.name, external.isEnum ? SymbolKind::Enum : SymbolKind::Class, 0, 0, {source_.size(), 0}, {}, external.name, "public"});
            if (!external.baseType.empty()) result_.baseTypes[external.name] = external.baseType;
            for (auto member : external.members) { member.span = {source_.size(), 0}; result_.members[external.isEnum ? "$type:" + external.name : external.name].push_back(std::move(member)); }
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
    static std::string BaseType(std::string type) { while (type.ends_with("[]")) type.resize(type.size() - 2); return type; }
    static bool IsOpenType(const std::string& type) { return type == "table" || type == "any"; }
    static bool IsNumeric(const std::string& type) { return type == "int" || type == "float"; }
    static bool IsFunctionType(const std::string& type) { return type.starts_with("$function:"); }
    static std::string FunctionType(size_t parameters, const std::string& result) { return "$function:" + std::to_string(parameters) + ":" + (result.empty() ? "any" : result); }
    static size_t FunctionArity(const std::string& type) { const auto end = type.find(':', 10); return static_cast<size_t>(std::stoull(type.substr(10, end - 10))); }
    static std::string FunctionResult(const std::string& type) { const auto end = type.find(':', 10); return end == std::string::npos ? "any" : type.substr(end + 1); }
    std::pair<size_t, size_t> Position(size_t offset) const {
        offset = (std::min)(offset, source_.size()); size_t line{}, lineStart{};
        for (size_t at{}; at < offset;) { const auto next = source_.find('\n', at); if (next == std::string_view::npos || next >= offset) break; ++line; at = lineStart = next + 1; }
        return {line, offset - lineStart};
    }
    size_t AddSymbol(std::string name, std::string type, SymbolKind kind, TextSpan span, std::vector<std::string> parameters = {}) {
        const auto [line, character] = Position(span.offset);
        result_.symbols.push_back({std::move(name), std::move(type), kind, line, character, span, std::move(parameters)});
        return result_.symbols.size() - 1;
    }
    void AddDiagnostic(std::string message, TextSpan span, DiagnosticSeverity severity = DiagnosticSeverity::Error) {
        const auto [line, character] = Position(span.offset); result_.diagnostics.push_back({std::move(message), line, character, severity, span});
    }
    void CheckType(const std::string& type, TextSpan span) {
        const auto base = BaseType(type);
        if (!base.empty() && base != "void" && !knownTypes_.contains(base)) AddDiagnostic("Unknown type '" + base + "'", span);
        if (type.ends_with("[]")) RegisterBuiltinType(type);
    }
    static std::vector<std::string> ParameterLabels(const BuiltinMember& member) {
        std::vector<std::string> result; for (const auto& parameter : member.parameters) result.push_back(parameter.type + " " + parameter.name + (parameter.optional ? "?" : "")); return result;
    }
    void RegisterBuiltinType(const std::string& type) {
        if (!registeredBuiltins_.insert(type).second) return;
        const auto add = [&](const BuiltinMember& member, const std::string& key) {
            result_.members[key].push_back({member.name, member.returnType, member.kind, 0, 0, {source_.size(), 0}, ParameterLabels(member)});
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
            knownTypes_.insert(value.name);
            const auto index = AddSymbol(value.name, value.name, SymbolKind::Class, value.span); result_.symbols[index].owner = value.name;
            if (!value.baseName.empty()) result_.baseTypes[value.name] = value.baseName;
        }
        for (const auto& value : result_.syntax.root.classes) if (!value.baseName.empty()) CheckType(value.baseName, value.span);
    }
    void DeclareMembers() {
        for (const auto& value : result_.syntax.root.enums) {
            std::unordered_set<std::string> names;
            if (value.members.empty()) AddDiagnostic("Enum '" + value.name + "' must declare at least one member", value.span);
            for (const auto& member : value.members) {
                if (!names.insert(member.name).second) AddDiagnostic("Enum member '" + member.name + "' is already declared on " + value.name, member.nameSpan);
                const auto index = AddSymbol(member.name, value.name, SymbolKind::Field, member.nameSpan);
                result_.symbols[index].owner = value.name; result_.members["$type:" + value.name].push_back(result_.symbols[index]);
            }
        }
        for (const auto& value : result_.syntax.root.classes) {
            std::unordered_set<std::string> names; bool hasConstructor = false;
            for (const auto& field : value.fields) {
                CheckType(field.type, field.span);
                if (!names.insert(field.name).second) AddDiagnostic("Member '" + field.name + "' is already declared on " + value.name, field.span);
                const auto index = AddSymbol(field.name, field.type, SymbolKind::Field, field.span);
                result_.symbols[index].owner = value.name; result_.symbols[index].access = field.access;
                result_.members[value.name].push_back(result_.symbols[index]);
            }
            for (const auto& function : value.functions) {
                if (function.isConstructor && function.name != value.name) AddDiagnostic("Constructor name must match class '" + value.name + "'", function.span);
                if (function.isConstructor && std::exchange(hasConstructor, true)) AddDiagnostic("Class '" + value.name + "' has more than one constructor", function.span);
                if (!function.isConstructor) {
                    CheckType(function.returnType, function.span);
                    if (!names.insert(function.name).second) AddDiagnostic("Member '" + function.name + "' is already declared on " + value.name, function.span);
                }
                std::vector<std::string> parameters;
                for (const auto& parameter : function.parameters) { CheckType(parameter.type, ParameterNameSpan(parameter)); parameters.push_back(parameter.type + " " + parameter.name); }
                const auto index = AddSymbol(function.name, function.isConstructor ? value.name : function.returnType, SymbolKind::Method, function.span, std::move(parameters));
                result_.symbols[index].owner = value.name; result_.symbols[index].access = function.access;
                if (!function.isConstructor) result_.members[value.name].push_back(result_.symbols[index]);
            }
        }
    }
    void FlattenInheritance() {
        for (const auto& [derived, firstBase] : result_.baseTypes) {
            std::unordered_set<std::string> visited{derived}; std::string base = firstBase;
            while (!base.empty()) {
                if (!visited.insert(base).second) { AddDiagnostic("Inheritance cycle involving '" + derived + "'", ClassSpan(derived)); break; }
                if (const auto found = result_.members.find(base); found != result_.members.end()) {
                    for (const auto& member : found->second) {
                        const auto own = std::find_if(result_.members[derived].begin(), result_.members[derived].end(), [&](const Symbol& value) { return value.name == member.name; });
                        const bool hidden = own != result_.members[derived].end();
                        if (hidden && member.access != "private" && (own->kind != member.kind || own->type != member.type || own->parameters != member.parameters)) AddDiagnostic("Override of '" + member.name + "' on '" + derived + "' does not match the inherited signature", own->span);
                        if (!hidden) result_.members[derived].push_back(member);
                    }
                }
                const auto parent = result_.baseTypes.find(base); base = parent == result_.baseTypes.end() ? "" : parent->second;
            }
        }
    }
    TextSpan ClassSpan(const std::string& name) const {
        const auto found = std::find_if(result_.syntax.root.classes.begin(), result_.syntax.root.classes.end(), [&](const auto& value) { return value.name == name; });
        return found == result_.syntax.root.classes.end() ? TextSpan{} : found->span;
    }
    const Symbol* FindMember(const std::string& type, std::string_view name) const {
        auto found = result_.members.find(type);
        if (found == result_.members.end()) found = result_.members.find(BaseType(type));
        if (found == result_.members.end()) return nullptr;
        const auto member = std::find_if(found->second.begin(), found->second.end(), [&](const Symbol& value) { return value.name == name; });
        return member == found->second.end() ? nullptr : &*member;
    }
    std::string ExpressionType(TextSpan span) const {
        const auto found = std::find_if(result_.expressions.rbegin(), result_.expressions.rend(), [&](const ExpressionInfo& value) { return value.span.offset == span.offset && value.span.length == span.length; });
        return found == result_.expressions.rend() ? std::string{} : found->type;
    }
    void CheckArguments(const Symbol& callable, const SyntaxExpression& call, const std::vector<std::string>& actual) {
        if (actual.size() != callable.parameters.size()) {
            AddDiagnostic("Expected " + std::to_string(callable.parameters.size()) + " argument(s) for '" + callable.name + "', got " + std::to_string(actual.size()), call.span);
            return;
        }
        for (size_t i{}; i < actual.size(); ++i) {
            const auto split = callable.parameters[i].find(' '); const auto expected = callable.parameters[i].substr(0, split);
            if (!Assignable(expected, actual[i])) AddDiagnostic("Argument " + std::to_string(i + 1) + " of '" + callable.name + "' expects '" + expected + "', got " + DisplayType(actual[i]), call.children[i + 1].span);
        }
    }
    void CheckBuiltinArguments(const BuiltinMember& callable, const SyntaxExpression& call, const std::vector<std::string>& actual) {
        const size_t required = static_cast<size_t>(std::count_if(callable.parameters.begin(), callable.parameters.end(), [](const auto& value) { return !value.optional; }));
        if (actual.size() < required || actual.size() > callable.parameters.size()) {
            AddDiagnostic("Expected " + std::to_string(required) + (required == callable.parameters.size() ? "" : " to " + std::to_string(callable.parameters.size())) + " argument(s) for '" + callable.name + "', got " + std::to_string(actual.size()), call.span); return;
        }
        for (size_t i{}; i < actual.size(); ++i) if (!Assignable(callable.parameters[i].type, actual[i])) AddDiagnostic("Argument " + std::to_string(i + 1) + " of '" + callable.name + "' expects '" + callable.parameters[i].type + "', got " + DisplayType(actual[i]), call.children[i + 1].span);
    }
    const Local* FindLocal(const std::vector<Local>& locals, std::string_view name) const {
        const auto found = std::find_if(locals.rbegin(), locals.rend(), [&](const Local& value) { return value.name == name; });
        return found == locals.rend() ? nullptr : &*found;
    }
    std::string Infer(const SyntaxExpression& expression, std::vector<Local>& locals, const SyntaxClass* owner, BoundExpression* output = nullptr) {
        using Kind = SyntaxExpression::Kind;
        BoundExpression scratch; auto& bound = output ? *output : scratch;
        bound.kind = expression.kind; bound.text = expression.text; bound.span = expression.span; bound.children.clear(); bound.parameters = expression.parameters; bound.lambdaBody.clear(); bound.lambdaIsBlock = expression.lambdaIsBlock;
        const auto child = [&](size_t index) {
            bound.children.emplace_back();
            return Infer(expression.children[index], locals, owner, &bound.children.back());
        };
        std::string type; std::optional<size_t> symbol; std::string lowering;
        switch (expression.kind) {
        case Kind::Missing: break;
        case Kind::Number: type = expression.text.find('.') == std::string::npos ? "int" : "float"; break;
        case Kind::String: type = "string"; break;
        case Kind::Boolean: type = "bool"; break;
        case Kind::Null: type = "null"; break;
        case Kind::Table: type = "table"; for (size_t i{}; i < expression.children.size(); ++i) child(i); break;
        case Kind::TableEntry:
            if (expression.text.empty()) { child(0); child(1); }
            else child(0);
            type = "any"; break;
        case Kind::New: {
            type = expression.text; CheckType(type, expression.span); std::vector<std::string> arguments; for (size_t i{}; i < expression.children.size(); ++i) arguments.push_back(child(i));
            if (enumTypes_.contains(type)) AddDiagnostic("Enum type '" + type + "' cannot be constructed", expression.span);
            const auto classNode = std::find_if(result_.syntax.root.classes.begin(), result_.syntax.root.classes.end(), [&](const auto& value) { return value.name == expression.text; });
            if (classNode != result_.syntax.root.classes.end()) {
                const auto constructor = std::find_if(classNode->functions.begin(), classNode->functions.end(), [](const auto& value) { return value.isConstructor; });
                const size_t expected = constructor == classNode->functions.end() ? 0 : constructor->parameters.size();
                if (arguments.size() != expected) AddDiagnostic("Expected " + std::to_string(expected) + " constructor argument(s) for '" + expression.text + "', got " + std::to_string(arguments.size()), expression.span);
                else if (constructor != classNode->functions.end()) for (size_t i{}; i < expected; ++i) if (!Assignable(constructor->parameters[i].type, arguments[i])) AddDiagnostic("Constructor argument " + std::to_string(i + 1) + " for '" + expression.text + "' expects '" + constructor->parameters[i].type + "', got " + DisplayType(arguments[i]), expression.children[i].span);
            }
            break;
        }
        case Kind::Identifier: {
            if (const auto* local = FindLocal(locals, expression.text)) { type = local->type; symbol = local->symbolIndex; }
            else if (owner) {
                if (const auto* member = FindMember(owner->name, expression.text)) {
                    type = member->type; symbol = SymbolIndex(*member); lowering = member->kind == SymbolKind::Method ? "implicit_method" : "implicit_field";
                    if (member->access == "private" && member->owner != owner->name) AddDiagnostic("Member '" + member->name + "' is private on '" + member->owner + "'", expression.span);
                }
            }
            if (type.empty() && (IsBuiltinType(expression.text) || enumTypes_.contains(expression.text))) type = "$type:" + expression.text;
            if (type.empty()) if (const auto builtin = FindBuiltinFunction(expression.text)) type = "$builtin:" + builtin->name;
            if (type.empty()) AddDiagnostic("Unknown identifier '" + expression.text + "'", expression.span);
            break;
        }
        case Kind::Parenthesized: type = child(0); break;
        case Kind::Member: {
            const auto target = child(0); const auto* member = FindMember(target, expression.text);
            if (member) {
                type = member->type; symbol = SymbolIndex(*member);
                if (member->access == "private" && (!owner || member->owner != owner->name)) AddDiagnostic("Member '" + member->name + "' is private on '" + member->owner + "'", MemberNameSpan(expression));
            }
            else if (target == "table" || target == "any") type = "any";
            else if (expression.text != "<missing>" && !target.empty() && !IsOpenType(target)) AddDiagnostic("Unknown member '" + expression.text + "' on " + BaseType(target), MemberNameSpan(expression), DiagnosticSeverity::Warning);
            break;
        }
        case Kind::Call: {
            const auto& callee = expression.children[0]; std::vector<std::string> arguments;
            if (callee.kind == Kind::Identifier && (callee.text == "int" || callee.text == "float" || callee.text == "bool" || callee.text == "string")) {
                child(0); for (size_t i = 1; i < expression.children.size(); ++i) arguments.push_back(child(i));
                if (arguments.size() != 1) AddDiagnostic("Cast to '" + callee.text + "' expects exactly one argument", expression.span);
                else if (const auto cast = BuiltinCastLowering(callee.text, arguments[0])) { type = callee.text; lowering = *cast; }
                else AddDiagnostic("Cannot convert " + DisplayType(arguments[0]) + " to '" + callee.text + "'", expression.children[1].span);
                break;
            }
            type = child(0);
            for (size_t i = 1; i < expression.children.size(); ++i) arguments.push_back(child(i));
            if (callee.kind == Kind::Identifier) if (const auto builtin = FindBuiltinFunction(callee.text)) {
                const size_t required = static_cast<size_t>(std::count_if(builtin->parameters.begin(), builtin->parameters.end(), [](const auto& value) { return !value.optional; }));
                if (arguments.size() < required || (!builtin->variadic && arguments.size() > builtin->parameters.size()))
                    AddDiagnostic("Expected " + std::to_string(required) + (builtin->variadic ? " or more" : "") + " argument(s) for '" + builtin->name + "', got " + std::to_string(arguments.size()), expression.span);
                const auto checked = (std::min)(arguments.size(), builtin->parameters.size());
                for (size_t i{}; i < checked; ++i) if (!Assignable(builtin->parameters[i].type, arguments[i])) AddDiagnostic("Argument " + std::to_string(i + 1) + " of '" + builtin->name + "' expects '" + builtin->parameters[i].type + "', got " + DisplayType(arguments[i]), expression.children[i + 1].span);
                type = builtin->returnType; lowering = builtin->lowering; break;
            }
            if (IsFunctionType(type)) {
                const auto expected = FunctionArity(type);
                if (arguments.size() != expected) AddDiagnostic("Expected " + std::to_string(expected) + " argument(s) for lambda, got " + std::to_string(arguments.size()), expression.span);
                type = FunctionResult(type); break;
            }
            const Symbol* callable = nullptr;
            std::optional<BuiltinMember> builtin;
            if (callee.kind == Kind::Member) {
                auto receiver = ExpressionType(callee.children[0].span); bool isStatic = receiver.starts_with("$type:"); if (isStatic) receiver = receiver.substr(6);
                builtin = FindBuiltinMember(receiver, callee.text, isStatic); callable = FindMember(ExpressionType(callee.children[0].span), callee.text);
            }
            else if (callee.kind == Kind::Identifier && owner) callable = FindMember(owner->name, callee.text);
            if (builtin) { CheckBuiltinArguments(*builtin, expression, arguments); lowering = builtin->lowering.empty() && builtin->isStatic ? "static_call" : builtin->lowering; }
            else if (callable && callable->kind == SymbolKind::Method) CheckArguments(*callable, expression, arguments);
            else if (!type.empty() && type != "any") AddDiagnostic("Expression of type " + DisplayType(type) + " is not callable", callee.span);
            break;
        }
        case Kind::Index: {
            const auto target = child(0); child(1);
            const auto index = ExpressionType(expression.children[1].span);
            if (target.ends_with("[]")) { type = target.substr(0, target.size() - 2); lowering = "list_index"; if (index != "int" && index != "any" && !index.empty()) AddDiagnostic("List index must be 'int', got " + DisplayType(index), expression.children[1].span); }
            else if (target == "table" || target == "any") type = "any";
            else if (!target.empty()) AddDiagnostic("Type '" + target + "' is not indexable", expression.span);
            break;
        }
        case Kind::Slice: {
            type = child(0); const auto first = child(1), last = child(2);
            if (!type.ends_with("[]")) AddDiagnostic("Only lists can be sliced", expression.span);
            if (first != "int" || last != "int") AddDiagnostic("Slice bounds must be 'int'", expression.span); break;
        }
        case Kind::Range: {
            const auto first = child(0), last = child(1); type = "$range";
            if (first != "int" || last != "int") AddDiagnostic("Range bounds must be 'int'", expression.span);
            break;
        }
        case Kind::Lambda: {
            auto lambdaLocals = locals; const size_t lambdaDepth = lambdaLocals.empty() ? 1 : lambdaLocals.back().depth + 1;
            for (const auto& parameter : expression.parameters) AddLocal(lambdaLocals, parameter.name, "any", SymbolKind::Parameter, parameter.span, expression.span, lambdaDepth);
            std::string resultType;
            if (expression.lambdaIsBlock) {
                bound.lambdaBody = AnalyzeStatements(expression.lambdaBody, lambdaLocals, owner, "$infer", expression.span.End(), lambdaDepth + 1);
                std::vector<std::string> returns; bool bareReturn = false;
                const auto collect = [&](auto&& self, const std::vector<BoundStatement>& statements) -> void {
                    for (const auto& statement : statements) {
                        if (statement.kind == SyntaxStatement::Kind::Return) {
                            if (statement.expression.kind == SyntaxExpression::Kind::Missing) bareReturn = true;
                            else returns.push_back(statement.expression.type);
                        }
                        self(self, statement.body); self(self, statement.elseBody);
                    }
                };
                collect(collect, bound.lambdaBody);
                if (returns.empty()) resultType = "void";
                else {
                    resultType = returns.front();
                    for (size_t i = 1; i < returns.size(); ++i) {
                        if (resultType == returns[i]) continue;
                        if ((resultType == "int" && returns[i] == "float") || (resultType == "float" && returns[i] == "int")) resultType = "float";
                        else if (resultType == "any" || returns[i] == "any") resultType = "any";
                        else if (Assignable(resultType, returns[i])) continue;
                        else if (Assignable(returns[i], resultType)) resultType = returns[i];
                        else { AddDiagnostic("Lambda return types are inconsistent", expression.span); resultType = "any"; break; }
                    }
                    if (bareReturn) AddDiagnostic("Lambda cannot mix value and valueless returns", expression.span);
                    if (!AlwaysReturns(expression.lambdaBody)) AddDiagnostic("Not all lambda code paths return a value", expression.span);
                }
            } else {
                bound.children.emplace_back(); resultType = Infer(expression.children[0], lambdaLocals, owner, &bound.children.back());
            }
            type = FunctionType(expression.parameters.size(), resultType); break;
        }
        case Kind::Unary: {
            const auto operand = child(0);
            if (expression.text == "!") { type = "bool"; if (operand != "bool" && operand != "any") AddDiagnostic("Operator '!' requires 'bool', got " + DisplayType(operand), expression.span); }
            else { type = operand; if (!IsNumeric(operand) && operand != "Vec3" && operand != "any") AddDiagnostic("Unary '" + expression.text + "' is not defined for " + DisplayType(operand), expression.span); }
            break;
        }
        case Kind::Binary: {
            const auto left = child(0), right = child(1);
            if (expression.text == "&&" || expression.text == "||") { type = "bool"; if ((left != "bool" && left != "any") || (right != "bool" && right != "any")) AddDiagnostic("Logical operator requires 'bool' operands", expression.span); }
            else if ((expression.text == "==" || expression.text == "!=") && (left == "null" || right == "null")) {
                const auto& other = left == "null" ? right : left; type = "bool";
                if (other == "int" || other == "float" || other == "bool") AddDiagnostic("Cannot compare 'null' with '" + other + "'", expression.span);
            }
            else if (left == "any" || right == "any") type = expression.text == "==" || expression.text == "!=" || expression.text == "<" || expression.text == ">" || expression.text == "<=" || expression.text == ">=" ? "bool" : "any";
            else if (const auto operation = FindBuiltinOperator(left, expression.text, right)) { type = operation->result; lowering = operation->lowering; }
            else if (!left.empty() && !right.empty()) AddDiagnostic("Operator '" + expression.text + "' is not defined for '" + left + "' and '" + right + "'", expression.span);
            break;
        }
        case Kind::Assignment: {
            const auto left = child(0), right = child(1); type = left;
            const auto targetKind = expression.children[0].kind;
            if (targetKind != Kind::Identifier && targetKind != Kind::Member && targetKind != Kind::Index) AddDiagnostic("Invalid assignment target", expression.children[0].span);
            if (expression.children[0].kind == Kind::Index && ExpressionType(expression.children[0].children[0].span).ends_with("[]")) lowering = "list_set";
            if (expression.text == "=") {
                if (!Assignable(left, right)) AddDiagnostic("Cannot assign " + DisplayType(right) + " to " + DisplayType(left), expression.children[1].span);
            } else if (const auto operation = FindBuiltinOperator(left, expression.text.substr(0, 1), right)) {
                if (!Assignable(left, operation->result)) AddDiagnostic("Result of '" + expression.text + "' is '" + operation->result + "', which cannot be assigned to '" + left + "'", expression.span);
                if (operation->lowering == "concat") lowering = "compound_concat";
            } else if (!left.empty() && !right.empty()) AddDiagnostic("Operator '" + expression.text + "' is not defined for '" + left + "' and '" + right + "'", expression.span);
            break;
        }
        }
        bound.type = type; bound.symbolIndex = symbol; bound.lowering = lowering;
        result_.expressions.push_back({expression.span, type, symbol, lowering}); return type;
    }
    static std::string DisplayType(const std::string& type) {
        if (IsFunctionType(type)) return "'lambda with " + std::to_string(FunctionArity(type)) + " parameter(s) returning " + FunctionResult(type) + "'";
        return type.empty() ? "unknown value" : "'" + type + "'";
    }
    bool Assignable(const std::string& expected, const std::string& actual) const {
        if (expected.empty() || actual.empty() || expected == "any" || actual == "any" || expected == actual) return true;
        if (actual == "null") return expected == "table" || expected.ends_with("[]") || (!IsBuiltinType(expected) && !enumTypes_.contains(expected) && expected != "void");
        if (expected.ends_with("[]") && actual == "table") return true;
        if (expected == "float" && actual == "int") return true;
        if (expected == "table" && actual.ends_with("[]")) return true;
        std::string current = actual;
        for (size_t depth{}; depth < result_.baseTypes.size() + 1;) { const auto base = result_.baseTypes.find(current); if (base == result_.baseTypes.end()) break; if (base->second == expected) return true; current = base->second; ++depth; }
        return false;
    }
    std::optional<size_t> SymbolIndex(const Symbol& symbol) const {
        for (size_t i{}; i < result_.symbols.size(); ++i) if (&result_.symbols[i] == &symbol || (result_.symbols[i].span.offset == symbol.span.offset && result_.symbols[i].name == symbol.name)) return i;
        return std::nullopt;
    }
    TextSpan MemberNameSpan(const SyntaxExpression& expression) const { return {expression.span.End() - expression.text.size(), expression.text.size()}; }
    void AddLocal(std::vector<Local>& locals, std::string name, std::string type, SymbolKind kind, TextSpan declaration, TextSpan visibility, size_t depth) {
        const auto duplicate = std::find_if(locals.rbegin(), locals.rend(), [&](const Local& local) { return local.depth == depth && local.name == name; });
        if (duplicate != locals.rend()) AddDiagnostic("Variable '" + name + "' is already declared in this scope", declaration);
        const auto index = AddSymbol(name, type, kind, declaration); locals.push_back({name, type, index, depth});
        result_.bindings.push_back({name, type, declaration, visibility, depth, index}); result_.variableTypes[name] = type;
    }
    std::vector<BoundStatement> AnalyzeStatements(const std::vector<SyntaxStatement>& statements, std::vector<Local>& locals, const SyntaxClass* owner, std::string_view returnType, size_t scopeEnd, size_t depth, size_t loopDepth = 0, bool preserveLocals = false) {
        std::vector<BoundStatement> boundStatements;
        const size_t startingLocals = locals.size();
        bool unreachable = false;
        for (const auto& statement : statements) {
            if (unreachable) AddDiagnostic("Unreachable code", statement.span, DiagnosticSeverity::Warning);
            BoundStatement bound; bound.kind = statement.kind; bound.name = statement.name; bound.secondName = statement.secondName; bound.type = statement.type; bound.inferredType = statement.inferredType; bound.span = statement.span;
            if (statement.kind == SyntaxStatement::Kind::Local) {
                const auto actual = Infer(statement.expression, locals, owner, &bound.expression); const auto type = statement.inferredType ? actual : statement.type; bound.type = type;
                const TextSpan nameSpan = statement.nameSpan;
                if (!statement.inferredType) {
                    CheckType(type, nameSpan); if (statement.expression.kind != SyntaxExpression::Kind::Missing && !Assignable(type, actual)) AddDiagnostic("Cannot assign " + DisplayType(actual) + " to '" + type + "'", statement.expression.span);
                    if (type.ends_with("[]") && statement.expression.kind == SyntaxExpression::Kind::Table) {
                        const auto element = type.substr(0, type.size() - 2);
                        for (size_t i{}; i < bound.expression.children.size(); ++i) {
                            if (bound.expression.children[i].kind == SyntaxExpression::Kind::TableEntry) AddDiagnostic("Typed list literals cannot contain keyed entries", statement.expression.children[i].span);
                            else if (!Assignable(element, bound.expression.children[i].type)) AddDiagnostic("List element " + std::to_string(i + 1) + " expects '" + element + "', got " + DisplayType(bound.expression.children[i].type), statement.expression.children[i].span);
                        }
                    }
                }
                if (statement.inferredType && type.empty()) AddDiagnostic("Cannot infer type of '" + statement.name + "'", nameSpan);
                if (statement.inferredType && type == "void") AddDiagnostic("Cannot initialize '" + statement.name + "' with a void expression", statement.expression.span);
                AddLocal(locals, statement.name, type, SymbolKind::Variable, nameSpan, {statement.span.offset, scopeEnd - statement.span.offset}, depth);
            } else if (statement.kind == SyntaxStatement::Kind::Return) {
                const auto actual = Infer(statement.expression, locals, owner, &bound.expression);
                if (returnType == "$infer") { }
                else if (returnType == "void" && statement.expression.kind != SyntaxExpression::Kind::Missing) AddDiagnostic("Void function cannot return a value", statement.expression.span);
                else if (returnType != "void" && statement.expression.kind == SyntaxExpression::Kind::Missing) AddDiagnostic("Function returning '" + std::string(returnType) + "' must return a value", statement.span);
                else if (returnType != "void" && !Assignable(std::string(returnType), actual)) AddDiagnostic("Cannot return " + DisplayType(actual) + " from function returning '" + std::string(returnType) + "'", statement.expression.span);
            } else if (statement.kind == SyntaxStatement::Kind::Break || statement.kind == SyntaxStatement::Kind::Continue) {
                if (loopDepth == 0) AddDiagnostic(std::string(statement.kind == SyntaxStatement::Kind::Break ? "'break'" : "'continue'") + " can only be used inside a loop", statement.span);
            } else if (statement.kind == SyntaxStatement::Kind::For) {
                const size_t loopLocals = locals.size();
                bound.initializer = AnalyzeStatements(statement.initializer, locals, owner, returnType, statement.span.End(), depth + 1, loopDepth, true);
                const auto condition = Infer(statement.expression, locals, owner, &bound.expression);
                if (condition != "bool" && condition != "any" && !condition.empty()) AddDiagnostic("For condition must be 'bool', got " + DisplayType(condition), statement.expression.span);
                Infer(statement.increment, locals, owner, &bound.increment);
                bound.body = AnalyzeStatements(statement.body, locals, owner, returnType, statement.span.End(), depth + 2, loopDepth + 1);
                locals.resize(loopLocals);
            } else if (statement.kind == SyntaxStatement::Kind::ForEach) {
                const size_t loopLocals = locals.size(); const auto iterable = Infer(statement.expression, locals, owner, &bound.expression);
                std::string firstType;
                if (iterable == "$range") {
                    firstType = "int";
                    if (!statement.secondName.empty()) AddDiagnostic("Range loops require exactly one iteration variable", statement.secondNameSpan);
                } else if (iterable.ends_with("[]")) {
                    firstType = statement.secondName.empty() ? iterable.substr(0, iterable.size() - 2) : "int";
                } else if (iterable == "table") {
                    firstType = "any";
                    if (statement.secondName.empty()) AddDiagnostic("Table loops require key and value iteration variables", statement.nameSpan);
                } else if (iterable == "any") {
                    firstType = "any";
                } else if (!iterable.empty()) AddDiagnostic("Type '" + iterable + "' is not iterable", statement.expression.span);
                AddLocal(locals, statement.name, firstType, SymbolKind::Variable, statement.nameSpan, {statement.nameSpan.offset, statement.span.End() - statement.nameSpan.offset}, depth + 1);
                if (!statement.secondName.empty()) {
                    const auto secondType = iterable.ends_with("[]") ? iterable.substr(0, iterable.size() - 2) : "any";
                    AddLocal(locals, statement.secondName, secondType, SymbolKind::Variable, statement.secondNameSpan, {statement.secondNameSpan.offset, statement.span.End() - statement.secondNameSpan.offset}, depth + 1);
                }
                bound.body = AnalyzeStatements(statement.body, locals, owner, returnType, statement.span.End(), depth + 2, loopDepth + 1);
                locals.resize(loopLocals);
            } else {
                const auto condition = Infer(statement.expression, locals, owner, &bound.expression);
                if (statement.kind == SyntaxStatement::Kind::If || statement.kind == SyntaxStatement::Kind::While) {
                    if (condition != "bool" && condition != "any" && !condition.empty()) AddDiagnostic(std::string(statement.kind == SyntaxStatement::Kind::If ? "If" : "While") + " condition must be 'bool', got " + DisplayType(condition), statement.expression.span);
                    bound.body = AnalyzeStatements(statement.body, locals, owner, returnType, statement.span.End(), depth + 1, loopDepth + (statement.kind == SyntaxStatement::Kind::While ? 1 : 0));
                    if (statement.kind == SyntaxStatement::Kind::If) bound.elseBody = AnalyzeStatements(statement.elseBody, locals, owner, returnType, statement.span.End(), depth + 1, loopDepth);
                }
            }
            boundStatements.push_back(std::move(bound));
            if (statement.kind == SyntaxStatement::Kind::Return || statement.kind == SyntaxStatement::Kind::Break || statement.kind == SyntaxStatement::Kind::Continue || (statement.kind == SyntaxStatement::Kind::If && !statement.elseBody.empty() && AlwaysReturns(statement.body) && AlwaysReturns(statement.elseBody))) unreachable = true;
        }
        if (!preserveLocals) locals.resize(startingLocals);
        return boundStatements;
    }
    void AnalyzeBodies() {
        result_.bound.span = result_.syntax.root.span;
        for (const auto& value : result_.syntax.root.enums) {
            BoundEnum boundEnum; boundEnum.name = value.name; boundEnum.span = value.span; long long nextValue{};
            for (const auto& member : value.members) {
                long long resolved = nextValue;
                if (member.initializer.kind != SyntaxExpression::Kind::Missing) {
                    const SyntaxExpression* number = &member.initializer; int sign = 1;
                    if (number->kind == SyntaxExpression::Kind::Unary && (number->text == "-" || number->text == "+") && number->children.size() == 1) { sign = number->text == "-" ? -1 : 1; number = &number->children[0]; }
                    if (number->kind != SyntaxExpression::Kind::Number || number->text.find('.') != std::string::npos) AddDiagnostic("Enum values must be integer constants", member.initializer.span);
                    else try { resolved = std::stoll(number->text) * sign; } catch (...) { AddDiagnostic("Enum value is outside the supported integer range", member.initializer.span); }
                }
                boundEnum.members.push_back({member.name, resolved, member.nameSpan}); nextValue = resolved + 1;
            }
            result_.bound.enums.push_back(std::move(boundEnum));
        }
        for (const auto& value : result_.syntax.root.classes) {
            if (!value.baseName.empty()) {
                const auto base = std::find_if(result_.syntax.root.classes.begin(), result_.syntax.root.classes.end(), [&](const auto& candidate) { return candidate.name == value.baseName; });
                if (base != result_.syntax.root.classes.end()) {
                    const auto constructor = std::find_if(base->functions.begin(), base->functions.end(), [](const auto& function) { return function.isConstructor; });
                    if (constructor != base->functions.end() && !constructor->parameters.empty()) AddDiagnostic("Base class '" + value.baseName + "' requires constructor arguments, but base constructor arguments are not supported", value.span);
                }
            }
            BoundClass boundClass; boundClass.name = value.name; boundClass.baseName = value.baseName; boundClass.span = value.span;
            for (const auto& field : value.fields) {
                BoundField boundField; boundField.access = field.access; boundField.type = field.type; boundField.name = field.name; boundField.span = field.span;
                if (field.initializer.kind != SyntaxExpression::Kind::Missing) {
                    std::vector<Local> none; const auto actual = Infer(field.initializer, none, &value, &boundField.initializer);
                    if (!Assignable(field.type, actual)) AddDiagnostic("Cannot assign " + DisplayType(actual) + " to '" + field.type + "'", field.initializer.span);
                } else { boundField.initializer.kind = SyntaxExpression::Kind::Missing; boundField.initializer.span = field.initializer.span; }
                boundClass.fields.push_back(std::move(boundField));
            }
            for (const auto& function : value.functions) {
                BoundFunction boundFunction; boundFunction.access = function.access; boundFunction.name = function.name; boundFunction.returnType = function.returnType; boundFunction.isConstructor = function.isConstructor; boundFunction.parameters = function.parameters; boundFunction.span = function.span;
                std::vector<Local> locals; const size_t scopeEnd = FunctionEnd(function);
                AddLocal(locals, "this", value.name, SymbolKind::Parameter, function.span, {function.span.offset, scopeEnd - function.span.offset}, 0);
                std::unordered_set<std::string> parameterNames;
                for (const auto& parameter : function.parameters) {
                    const auto nameSpan = ParameterNameSpan(parameter);
                    if (!parameterNames.insert(parameter.name).second) AddDiagnostic("Parameter '" + parameter.name + "' is already declared", nameSpan);
                    AddLocal(locals, parameter.name, parameter.type, SymbolKind::Parameter, nameSpan, {nameSpan.offset, scopeEnd - nameSpan.offset}, 0);
                }
                boundFunction.body = AnalyzeStatements(function.body, locals, &value, function.isConstructor ? "void" : function.returnType, scopeEnd, 1);
                if (!function.isConstructor && function.returnType != "void" && !AlwaysReturns(function.body)) AddDiagnostic("Not all code paths return a value from '" + function.name + "'", function.span);
                boundClass.functions.push_back(std::move(boundFunction));
            }
            result_.bound.classes.push_back(std::move(boundClass));
        }
        std::vector<Local> locals; result_.bound.topLevel = AnalyzeStatements(result_.syntax.root.topLevel, locals, nullptr, "void", source_.size(), 0);
    }
    size_t FunctionEnd(const SyntaxFunction& function) const {
        size_t end = function.span.End();
        const auto visit = [&](auto&& self, const std::vector<SyntaxStatement>& statements) -> void { for (const auto& statement : statements) { end = (std::max)(end, statement.span.End()); self(self, statement.body); self(self, statement.elseBody); } };
        visit(visit, function.body); return end;
    }
    bool AlwaysReturns(const std::vector<SyntaxStatement>& statements) const {
        for (const auto& statement : statements) {
            if (statement.kind == SyntaxStatement::Kind::Return) return true;
            if (statement.kind == SyntaxStatement::Kind::If && !statement.elseBody.empty() && AlwaysReturns(statement.body) && AlwaysReturns(statement.elseBody)) return true;
        }
        return false;
    }

    std::string_view source_;
    Analysis result_;
    std::unordered_set<std::string> knownTypes_;
    std::unordered_set<std::string> declaredTypes_;
    std::unordered_set<std::string> registeredBuiltins_;
    std::unordered_set<std::string> enumTypes_;
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
    if (best) return best->type;
    const auto legacy = analysis.variableTypes.find(std::string(name)); return legacy == analysis.variableTypes.end() ? std::nullopt : std::optional<std::string>(legacy->second);
}

const ExpressionInfo* FindExpression(const Analysis& analysis, size_t position) {
    const ExpressionInfo* best = nullptr;
    for (const auto& expression : analysis.expressions) if (expression.span.Contains(position) && (!best || expression.span.length < best->span.length)) best = &expression;
    return best;
}

} // namespace luasharp
