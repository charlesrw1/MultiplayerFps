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

// Value types (structs) are spelled by value, reference types as pointers.
void AppendCType(std::string& out, std::string_view type, const StringSet& enums, const StringSet& structs) {
    if (const auto scalar = ScalarCType(type, enums)) { out += *scalar; return; }
    if (type.ends_with("[]")) { out += "LS_List*"; return; }
    if (structs.contains(type)) { out += "LS_"; AppendIdentifier(out, type); return; }
    if (IsBuiltinType(type)) throw std::runtime_error("Native C backend requires a registered ABI for builtin type '" + std::string(type) + "'");
    out += "LS_"; AppendIdentifier(out, type); out += '*';
}

// Output fragments that Put() appends in place.
struct Id { std::string_view value; };                         // sanitized C identifier
struct Fn { std::string_view owner; std::string_view name; };  // ls_<owner>_<name>
struct CTypeName { std::string_view type; };                   // C spelling of a LuaSharp type

// Operand index of ls_value_arithmetic.
int ArithmeticIndex(char operation) {
    switch (operation) {
    case '+': return 0; case '-': return 1; case '*': return 2; case '/': return 3; case '%': return 4;
    default: throw std::runtime_error(std::string("Native C backend has no dynamic operator '") + operation + "'");
    }
}

struct CEmitter {
    const Analysis& analysis;
    const BoundProgram& program;
    const TypeTable& types;
    const std::vector<ExternalType>& externalTypes;
    StringSet enums;
    StringSet externalNames;
    StringSet externalStructs;
    StringSet structNames;                  // user and native value types
    StringMap<const BoundClass*> classes;   // reference types
    StringMap<const BoundClass*> structs;   // user value types
    StringMap<const ExternalType*> natives; // extern classes: host proxies (LS_NativeObject)
    std::vector<const ExternalType*> orderedNatives; // each after its base
    struct FunctionRef { const BoundClass* owner{}; const BoundFunction* function{}; std::string cName; size_t ordinal{}; };
    std::unordered_map<size_t, FunctionRef> functionRefs; // symbol index -> emitted function
    std::vector<const BoundClass*> orderedClasses;         // reference types, each after its base
    // One vtable entry. introducer declared the virtual method and fixes the C signature; impl is null while abstract.
    struct Slot { std::string key; const BoundClass* introducer{}; const BoundFunction* impl{}; const BoundClass* implOwner{}; };
    std::unordered_map<const BoundClass*, std::vector<Slot>> vtables;
    std::unordered_map<size_t, size_t> slotOfSymbol;       // virtual method symbol -> vtable index
    std::unordered_set<size_t> globalSymbols;
    std::vector<const VariableBinding*> bindingsBySymbol;          // first binding of each symbol index
    std::unordered_map<size_t, const VariableBinding*> bindingAt;  // declaration offset -> binding
    std::vector<const BoundExpression*> lambdas;
    std::unordered_map<const BoundExpression*, size_t> lambdaIndices;
    std::vector<std::vector<const VariableBinding*>> lambdaCaptures;
    std::vector<std::string_view> lambdaOwners;  // enclosing class of each lambda, empty at top level
    std::unordered_set<size_t> cellSymbols;      // captured locals that are also assigned
    std::unordered_set<size_t> inoutSymbols;     // inout parameters: C pointers to the caller's copy
    StringMap<bool> structReferences;            // memo for HasReferences
    std::vector<std::pair<const BoundClass*, const BoundField*>> staticFields; // declaration order
    std::vector<std::string_view> dynamicNames;  // member names used on table/any values
    std::vector<std::string_view> typeOfTargets; // types named by typeof: each has a slot "ls_typeof_<T>" that the module traces
    const Symbol* typeOfProvider{};              // the [TypeOf] method that fills those slots
    std::vector<std::pair<const ExternalType*, const Symbol*>> baseThunks; // `base.m()` of extern virtual methods: (nearest extern class, m)
    std::optional<size_t> activeLambda;
    // Arguments of the inout call being emitted, already in ls_argument_<label>_<i>.
    struct HoistedArguments { const BoundExpression* call{}; size_t label{}; std::vector<bool> inout; };
    HoistedArguments hoisted;
    std::string_view constructorResult;         // variable a bare `return;` yields inside a constructor
    TypeId functionReturn = TypeTable::Unknown; // return type of the method being emitted
    std::string output;
    size_t nextLabel{};
    // Debug builds: the instrumented function being emitted. Its text goes to a buffer until its locals struct is known.
    const CEmitOptions& emitOptions;
    struct DebugFrame {
        std::string savedOutput;
        std::string prologue;                                    // after the ls_locals declaration: parameter copies
        std::unordered_map<size_t, std::string> fields;          // symbol -> field of ls_locals
        std::vector<std::pair<std::string, std::string>> layout; // C type and field of each debug local, in order
        std::optional<uint32_t> statement;                       // the last statement id
    };
    std::optional<DebugFrame> debugFrame;
    std::vector<size_t> lineStarts;
    std::string lineFile; // sourceName as the body of a C string
    NativeDebugInfo debugInfo;
    std::vector<std::string> debugSymbols; // C name of each instrumented function

    CEmitter(const Analysis& value, const std::vector<ExternalType>& external, const CEmitOptions& options)
        : analysis(value), program(value.bound), types(value.types), externalTypes(external), emitOptions(options) {
        if (options.debugHooks) {
            assert(!options.source.empty() && !options.sourceName.empty());
            lineStarts.push_back(0);
            for (size_t i{}; i < options.source.size(); ++i) if (options.source[i] == '\n') lineStarts.push_back(i + 1);
            for (const char character : options.sourceName) { if (character == '\\' || character == '"') lineFile += '\\'; lineFile += character; }
            debugInfo.sourceName = options.sourceName;
        }
        for (const auto& item : program.enums) enums.insert(item.name);
        for (const auto& item : NativeStructTypes()) structNames.insert(item);
        for (const auto& item : program.classes) {
            (item.isStruct ? structs : classes)[item.name] = &item;
            if (item.isStruct) structNames.insert(item.name);
            AssignFunctionNames(item);
            for (const auto& field : item.fields) if (field.isStatic) staticFields.emplace_back(&item, &field);
        }
        for (const auto& item : externalTypes) {
            externalNames.insert(item.name); if (item.isEnum) enums.insert(item.name); if (item.isStruct) externalStructs.insert(item.name);
            if (item.isNative) natives.emplace(item.name, &item);
        }
        OrderNatives();
        OrderClasses();
        BuildVtables();
        bindingsBySymbol.resize(analysis.symbols.size());
        for (const auto& binding : analysis.bindings) {
            assert(binding.symbolIndex < bindingsBySymbol.size());
            if (!bindingsBySymbol[binding.symbolIndex]) bindingsBySymbol[binding.symbolIndex] = &binding;
            bindingAt.emplace(binding.declaration.offset, &binding);
        }
        for (const auto& item : program.classes) for (const auto& function : item.functions) for (const auto& parameter : function.parameters)
            if (const auto* binding = ParameterBinding(parameter); binding && parameter.isInout) inoutSymbols.insert(binding->symbolIndex);
        for (const auto& statement : program.Statements(program.topLevel)) {
            if (statement.kind != SyntaxStatement::Kind::Local) continue;
            if (const auto* binding = BindingAt(statement.syntax->nameSpan.offset)) globalSymbols.insert(binding->symbolIndex);
        }
        CollectLambdas();
        CaptureLambdas();
        std::unordered_set<size_t> captured;
        for (const auto& captures : lambdaCaptures) for (const auto* binding : captures) if (binding->name != "this") captured.insert(binding->symbolIndex);
        // A captured struct may be changed through its members or methods, so it always lives in a shared cell.
        for (const auto symbol : captured) if (const auto* binding = Binding(symbol); binding && IsStruct(binding->type)) cellSymbols.insert(symbol);
        for (const auto& item : program.expressions) {
            const auto writes = [&](const BoundExpression& written) {
                const auto& target = Unparenthesized(written);
                if (target.kind == SyntaxExpression::Kind::Identifier && target.symbolIndex && captured.contains(*target.symbolIndex)) cellSymbols.insert(*target.symbolIndex);
            };
            if (item.kind == SyntaxExpression::Kind::Assignment) writes(Child(item, 0));
            if (const auto* function = InoutFunction(item)) for (size_t i{}; i < function->parameters.size(); ++i) if (function->parameters[i].isInout) writes(InoutTarget(item, i));
            const bool dynamicMember = item.kind == SyntaxExpression::Kind::Member && IsDynamic(Child(item, 0).type);
            const bool keyedEntry = item.kind == SyntaxExpression::Kind::TableEntry && !item.Text().empty();
            if ((dynamicMember || keyedEntry) && std::find(dynamicNames.begin(), dynamicNames.end(), item.Text()) == dynamicNames.end()) dynamicNames.push_back(item.Text());
            if (const auto thunk = NativeBaseCall(item); thunk.first && std::find(baseThunks.begin(), baseThunks.end(), thunk) == baseThunks.end()) baseThunks.push_back(thunk);
            if (item.kind == SyntaxExpression::Kind::TypeOf && std::find(typeOfTargets.begin(), typeOfTargets.end(), item.Text()) == typeOfTargets.end()) {
                assert(item.symbolIndex && analysis.symbols[*item.symbolIndex].isTypeOf);
                typeOfProvider = &analysis.symbols[*item.symbolIndex];
                typeOfTargets.push_back(item.Text());
            }
        }
    }
    // `base.m()` where m is an extern virtual method: the host calls the extern implementation without virtual dispatch.
    std::pair<const ExternalType*, const Symbol*> NativeBaseCall(const BoundExpression& call) const {
        if (call.kind != SyntaxExpression::Kind::Call) return {};
        const auto& callee = Child(call, 0);
        if (callee.kind != SyntaxExpression::Kind::Member || Child(callee, 0).lowering != Lowering::Base) return {};
        const auto* member = NativeMember(call);
        if (!member || !member->isVirtual || member->isStatic) return {};
        const auto* native = NativeBaseOf(Name(Child(callee, 0).type));
        assert(native);
        return {native, member};
    }
    static std::string BaseThunkName(std::string_view method) { return "_base_" + std::string(method); }

    // The script base class; null at the root of the script chain, also when the base is an extern class.
    const BoundClass* BaseOf(const BoundClass& type) const {
        if (type.isStruct || type.baseName.empty() || natives.contains(type.baseName)) return nullptr;
        const auto found = classes.find(type.baseName);
        if (found == classes.end()) throw std::runtime_error("Native C backend cannot derive '" + type.name + "' from external type '" + type.baseName + "' yet");
        return found->second;
    }
    // The nearest extern ancestor of a script class, or null. Its objects start with LS_NativeObject.
    const ExternalType* NativeBaseOf(const BoundClass& type) const {
        const auto* root = &type;
        while (const auto* base = BaseOf(*root)) root = base;
        const auto found = natives.find(root->baseName);
        return found == natives.end() ? nullptr : found->second;
    }
    const ExternalType* NativeBaseOf(std::string_view type) const {
        if (const auto found = natives.find(type); found != natives.end()) return found->second;
        if (const auto found = classes.find(type); found != classes.end()) return NativeBaseOf(*found->second);
        return nullptr;
    }
    // Extern objects and script objects that derive from extern classes: null once the host destroys them.
    bool IsNativeBacked(TypeId type) const { return NativeBaseOf(Name(type)) != nullptr; }
    // The base of a script or extern class; empty at the root.
    std::string_view BaseName(std::string_view type) const {
        if (const auto found = classes.find(type); found != classes.end()) return found->second->baseName;
        if (const auto found = natives.find(type); found != natives.end()) return found->second->baseType;
        return {};
    }
    bool IsSubclass(std::string_view derived, std::string_view base) const {
        for (auto type = derived; !type.empty(); type = BaseName(type)) if (type == base) return true;
        return false;
    }
    void OrderNatives() {
        std::unordered_set<const ExternalType*> done;
        const auto visit = [&](auto&& self, const ExternalType& type) -> void {
            if (!done.insert(&type).second) return;
            if (!type.baseType.empty()) {
                const auto base = natives.find(type.baseType);
                if (base == natives.end()) throw std::runtime_error("Extern class '" + type.name + "' derives from '" + type.baseType + "', which is not an extern class");
                self(self, *base->second);
            }
            orderedNatives.push_back(&type);
        };
        for (const auto& item : externalTypes) if (item.isNative) visit(visit, item);
    }
    void OrderClasses() {
        std::unordered_set<const BoundClass*> done;
        const auto visit = [&](auto&& self, const BoundClass& type) -> void {
            if (done.contains(&type)) return;
            if (const auto* base = BaseOf(type)) self(self, *base);
            done.insert(&type); orderedClasses.push_back(&type);
        };
        for (const auto& item : program.classes) if (!item.isStruct) visit(visit, item);
    }
    // Instance fields in layout order: the base's fields first.
    std::vector<const BoundField*> InstanceFields(const BoundClass& type) const {
        std::vector<const BoundField*> result;
        if (const auto* base = BaseOf(type)) result = InstanceFields(*base);
        for (const auto& field : type.fields) if (!field.isStatic) result.push_back(&field);
        return result;
    }
    static std::string SlotKey(const BoundFunction& function) {
        std::string key = function.name + '(';
        for (const auto& parameter : function.parameters) { if (parameter.isInout) key += "inout "; key += parameter.type; key += ','; }
        return key + ')';
    }
    // A class starts with its base's slots. An override replaces the entry of its slot; a new virtual method appends one.
    void BuildVtables() {
        for (const auto* type : orderedClasses) {
            const auto* base = BaseOf(*type);
            auto slots = base ? vtables.at(base) : std::vector<Slot>{};
            for (const auto& function : type->functions) {
                if (function.isConstructor || function.isStatic || !function.isVirtual) continue;
                const auto key = SlotKey(function);
                const auto* impl = function.isAbstract ? nullptr : &function;
                auto existing = std::find_if(slots.begin(), slots.end(), [&](const Slot& slot) { return slot.key == key; });
                if (existing == slots.end()) existing = slots.insert(slots.end(), Slot{key, type, impl, type});
                else { existing->impl = impl; existing->implOwner = type; }
                slotOfSymbol[function.symbolIndex] = static_cast<size_t>(existing - slots.begin());
            }
            vtables[type] = std::move(slots);
        }
    }

    void CollectLambdas() {
        std::string_view owner;
        const auto expression = [&](auto&& self, const BoundExpression& value) -> void {
            if (value.kind == SyntaxExpression::Kind::Lambda) { lambdaIndices.emplace(&value, lambdas.size()); lambdas.push_back(&value); lambdaOwners.push_back(owner); }
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
            owner = type.name;
            for (const auto& field : type.fields) expression(expression, program.Expression(field.initializer));
            for (const auto& function : type.functions) statements(statements, function.body);
        }
    }

    // A lambda captures every non-global variable it or a nested lambda references from outside its own span.
    void CaptureLambdas() {
        for (const auto* lambda : lambdas) {
            std::vector<const VariableBinding*> captured;
            const auto add = [&](const VariableBinding* binding) {
                if (!binding || globalSymbols.contains(binding->symbolIndex)) return;
                if (std::none_of(captured.begin(), captured.end(), [&](const auto* value) { return value->symbolIndex == binding->symbolIndex; })) captured.push_back(binding);
            };
            const auto scanExpression = [&](auto&& self, const BoundExpression& value) -> void {
                if (value.symbolIndex) {
                    const auto* binding = Binding(*value.symbolIndex);
                    if (binding && (binding->declaration.offset < lambda->span.offset || binding->declaration.offset >= lambda->span.End())) add(binding);
                }
                if (value.lowering == Lowering::ImplicitField || value.lowering == Lowering::ImplicitMethod || value.lowering == Lowering::Base) {
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

    // ---- debug builds ----
    uint32_t SourceLine(size_t offset) const { return static_cast<uint32_t>(std::upper_bound(lineStarts.begin(), lineStarts.end(), offset) - lineStarts.begin()); }
    size_t DebugFunctionIndex() const { assert(debugFrame); return debugInfo.functions.size() - 1; }
    const std::string* DebugField(size_t symbol) const {
        if (!debugFrame) return nullptr;
        const auto found = debugFrame->fields.find(symbol);
        return found == debugFrame->fields.end() ? nullptr : &found->second;
    }
    std::string CType(std::string_view type) const { std::string result; AppendCType(result, type, enums, structNames); return result; }
    // Adds a field to ls_locals, visible from the next statement. symbol is empty for `this`.
    const std::string& AddDebugLocal(std::optional<size_t> symbol, std::string_view name, std::string cType, std::string_view type, bool indirect) {
        assert(debugFrame);
        auto field = symbol ? Identifier(name) + "__" + std::to_string(*symbol) : std::string("ls_self");
        debugFrame->layout.emplace_back(std::move(cType), field);
        const uint32_t from = debugFrame->statement ? *debugFrame->statement + 1 : 0;
        debugInfo.functions.back().locals.push_back({std::string(name), std::string(type), from, UINT32_MAX, indirect});
        if (!symbol) return debugFrame->layout.back().second;
        const auto [entry, added] = debugFrame->fields.emplace(*symbol, std::move(field));
        assert(added);
        return entry->second;
    }
    // Parameters are copied into ls_locals; ParameterCells adds the cells.
    void DebugParameters(const std::vector<SyntaxParameter>& parameters) {
        if (!debugFrame) return;
        for (const auto& parameter : parameters) {
            const auto* binding = ParameterBinding(parameter);
            if (!binding || IsCell(binding)) continue;
            const auto& field = AddDebugLocal(binding->symbolIndex, parameter.name, CType(parameter.type) + (parameter.isInout ? "*" : ""), parameter.type, parameter.isInout);
            debugFrame->prologue += "    ls_locals." + field + " = " + Identifier(parameter.name) + ";\n";
        }
    }
    // `self` as the local `this`; after `self` is declared.
    void DebugSelf(size_t depth, std::string cType, std::string_view type, bool indirect) {
        if (!debugFrame) return;
        AddDebugLocal(std::nullopt, "this", std::move(cType), type, indirect);
        Line(depth, "ls_locals.ls_self = self;");
    }
    uint32_t NewStatement(size_t offset) {
        assert(debugFrame);
        auto& lines = debugInfo.functions.back().lines;
        lines.push_back(SourceLine(offset));
        debugFrame->statement = static_cast<uint32_t>(lines.size() - 1);
        return *debugFrame->statement;
    }
    void LineDirective(size_t offset) { if (debugFrame) Line(0, "#line ", SourceLine(offset), " \"", lineFile, '"'); }
    void HookCall(uint32_t statement) { Put("LS_DEBUG(", DebugFunctionIndex(), ", ", statement, ')'); }
    // The hook before a statement.
    void DebugStatement(size_t depth, size_t offset) {
        if (!debugFrame) return;
        LineDirective(offset);
        const auto statement = NewStatement(offset);
        Line(depth, [&] { HookCall(statement); }, ';');
    }
    // A loop condition that runs the hook before each test.
    template <typename Test> auto LoopCondition(size_t offset, const Test& condition) {
        return [this, offset, &condition] {
            if (!debugFrame) return condition();
            const auto statement = NewStatement(offset);
            Put('(', [&] { HookCall(statement); }, ", ", condition, ')');
        };
    }
    // Locals added after the mark stop being visible at the next statement id.
    size_t DebugScope() const { return debugFrame ? debugInfo.functions.back().locals.size() : 0; }
    void CloseDebugScope(size_t mark) {
        if (!debugFrame) return;
        auto& function = debugInfo.functions.back();
        for (size_t i = mark; i < function.locals.size(); ++i) if (function.locals[i].visibleTo == UINT32_MAX) function.locals[i].visibleTo = static_cast<uint32_t>(function.lines.size());
    }
    // Emits one function. In debug builds its locals live in "LS_DebugLocals_<i> ls_locals", declared after the first line of its text.
    template <typename Body> void DebugFunction(std::string name, std::string symbol, const Body& body) {
        if (!emitOptions.debugHooks) return body();
        assert(!debugFrame);
        debugInfo.functions.push_back({std::move(name), {}, {}});
        debugSymbols.push_back(std::move(symbol));
        debugFrame.emplace();
        debugFrame->savedOutput = std::exchange(output, {});
        body();
        CloseDebugScope(0);
        auto frame = std::move(*debugFrame);
        debugFrame.reset();
        const auto text = std::exchange(output, std::move(frame.savedOutput));
        const auto index = debugInfo.functions.size() - 1;
        Line(0, "typedef struct LS_DebugLocals_", index, " {");
        Line(1, "ls_wrap_bits ls_statement;");
        for (const auto& [type, field] : frame.layout) Line(1, type, ' ', field, ';');
        Line(0, "} LS_DebugLocals_", index, ';');
        const auto open = text.find('\n') + 1;
        assert(open > 0);
        output.append(text, 0, open);
        Line(1, "LS_DebugLocals_", index, " ls_locals; ls_locals.ls_statement = (ls_wrap_bits)-1;");
        output += frame.prologue;
        output.append(text, open);
        Line(0, LineReset);
        Line(0, "static void ls_debug_end_", index, "(void) {}");
        if (!frame.layout.empty())
            Line(0, "static const ls_wrap_bits ls_debug_offsets_", index, "[] = {", [&] { for (const auto& item : frame.layout) Put("ls_offsetof(LS_DebugLocals_", index, ", ", item.second, "), "); }, "};");
    }
    static constexpr std::string_view LineReset = "#line ls_c";
    // The module table (LS_DebugModule). Then each LineReset becomes a #line of the generated C.
    void DebugTables() {
        assert(!debugSymbols.empty());
        Line(0, "static void ls_debug_text_end(void) {}");
        Line(0, "static const LS_DebugFunction ls_debug_functions[] = {");
        for (size_t i{}; i < debugSymbols.size(); ++i)
            Line(1, "{(const void*)", debugSymbols[i], ", (const void*)ls_debug_end_", i, ", ", [&] { if (debugInfo.functions[i].locals.empty()) Put("(const ls_wrap_bits*)0"); else Put("ls_debug_offsets_", i); }, ", ", debugInfo.functions[i].locals.size(), "},");
        Line(0, "};");
        Line(0, "static const LS_DebugModule ls_debug_module = {&ls_debug, ls_debug_functions, ", debugSymbols.size(), ", (const void*)ls_debug_text_begin, (const void*)ls_debug_text_end};");
        Line(0, "const LS_DebugModule* ls_module_debug(void) { return &ls_debug_module; }");
        std::string resolved;
        resolved.reserve(output.size());
        size_t physicalLine = 1;
        for (size_t start{}; start < output.size();) {
            const auto end = output.find('\n', start);
            const auto line = std::string_view(output).substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (line == LineReset) resolved += "#line " + std::to_string(physicalLine + 1) + " \"" + lineFile + ".c\"";
            else resolved += line;
            if (end == std::string::npos) break;
            resolved += '\n'; start = end + 1; ++physicalLine;
        }
        output = std::move(resolved);
    }

    // Appends each part to output. Invocable parts emit themselves.
    template <typename... Parts> void Put(const Parts&... parts) { (Emit(parts), ...); }
    template <typename... Parts> void Line(size_t depth, const Parts&... parts) { output.append(depth * 4, ' '); Put(parts...); output += '\n'; }
    template <typename T> void Emit(const T& part) {
        static_assert(!std::is_same_v<T, bool>);
        if constexpr (std::is_invocable_v<const T&>) part();
        else if constexpr (std::is_same_v<T, Id>) AppendIdentifier(output, part.value);
        else if constexpr (std::is_same_v<T, Fn>) AppendFunction(output, part.owner, part.name);
        else if constexpr (std::is_same_v<T, CTypeName>) AppendCType(output, part.type, enums, structNames);
        else if constexpr (std::is_same_v<T, char>) output += part;
        else if constexpr (std::is_integral_v<T>) { char buffer[24]; output.append(buffer, std::to_chars(buffer, std::end(buffer), part).ptr); }
        else output.append(std::string_view(part));
    }
    auto Emitted(const BoundExpression& value, std::string_view owner) { return [this, &value, owner] { Expression(value, owner); }; }
    auto Boxed(const BoundExpression& value, std::string_view owner) { return [this, &value, owner] { Box(value, owner); }; }
    auto Converted(TypeId target, const BoundExpression& value, std::string_view owner) { return [this, target, &value, owner] { Value(target, value, owner); }; }
    // Unique statement-expression temporary "ls_<name>_<label>".
    auto Temp(std::string_view name, size_t label) { return [this, name, label] { Put("ls_", name, '_', label); }; }
    // Boxed interned member name.
    auto NameValue(std::string_view name) { return [this, name] { Put("ls_box_pointer(LS_KIND_STRING, LS_NAME(", Id{name}, "))"); }; }

    const std::string& Name(TypeId type) const { return types.Name(type); }
    CTypeName Type(TypeId type) const { return {Name(type)}; }
    TypeId Find(std::string_view type) const { return types.Find(type).value_or(TypeTable::Unknown); }
    bool IsEnum(TypeId type) const { return enums.contains(Name(type)); }
    bool IsClass(TypeId type) const { return classes.contains(Name(type)); }
    bool IsNative(TypeId type) const { return natives.contains(Name(type)); }
    bool IsObject(TypeId type) const { return IsClass(type) || IsNative(type); } // values start with an LS_TypeInfo pointer
    bool IsDynamic(TypeId type) const { return type == TypeTable::Table || type == TypeTable::Any; }
    bool IsStruct(TypeId type) const { return structNames.contains(Name(type)); }
    bool IsUserStruct(std::string_view name) const { return structs.contains(name); }
    const BoundClass* UserType(std::string_view name) const {
        if (const auto found = classes.find(name); found != classes.end()) return found->second;
        if (const auto found = structs.find(name); found != structs.end()) return found->second;
        return nullptr;
    }
    // The first function of a name keeps the plain symbol; later overloads get "__<ordinal>" so exported names stay stable.
    void AssignFunctionNames(const BoundClass& type) {
        StringMap<size_t> ordinals;
        for (const auto& function : type.functions) {
            const std::string key = function.isConstructor ? "new" : function.name;
            const auto ordinal = ordinals[key]++;
            auto name = FunctionName(type.name, key);
            if (ordinal) name += "__" + std::to_string(ordinal);
            functionRefs.emplace(function.symbolIndex, FunctionRef{&type, &function, std::move(name), ordinal});
        }
    }
    const FunctionRef* FindFunctionRef(const BoundExpression& value) const {
        if (!value.symbolIndex) return nullptr;
        const auto found = functionRefs.find(*value.symbolIndex);
        return found == functionRefs.end() ? nullptr : &found->second;
    }
    // True when the bound expression names storage, so `&` of it is valid.
    bool IsAddressable(const BoundExpression& value) const {
        switch (value.kind) {
        case SyntaxExpression::Kind::Identifier: return types.Kind(value.type) != TypeKind::Meta;
        case SyntaxExpression::Kind::Parenthesized: return IsAddressable(Child(value, 0));
        case SyntaxExpression::Kind::Member:
            if (value.lowering == Lowering::StaticField) return true;
            if (value.lowering == Lowering::NativeConstant || types.Kind(Child(value, 0).type) == TypeKind::Meta) return false;
            return IsStruct(Child(value, 0).type) ? IsAddressable(Child(value, 0)) : !IsDynamic(Child(value, 0).type);
        case SyntaxExpression::Kind::Index: return value.lowering == Lowering::ListIndex;
        default: return false;
        }
    }
    bool IsManagedObject(std::string_view type) const { return type == "string" || type == "table" || type == "any" || type.starts_with("$function:") || type.ends_with("[]") || classes.contains(type) || natives.contains(type); }
    const BoundExpression& Child(const BoundExpression& value, size_t index) const { return program.Child(value, index); }
    const VariableBinding* Binding(size_t symbol) const { assert(symbol < bindingsBySymbol.size()); return bindingsBySymbol[symbol]; }
    const VariableBinding* BindingAt(size_t offset) const { const auto found = bindingAt.find(offset); return found == bindingAt.end() ? nullptr : found->second; }
    // Parameter bindings start at the parameter name.
    const VariableBinding* ParameterBinding(const SyntaxParameter& parameter) const { return BindingAt(parameter.span.End() - parameter.name.size()); }
    bool IsCell(const VariableBinding* binding) const { return binding && cellSymbols.contains(binding->symbolIndex); }
    bool CapturedByActive(size_t symbol) const {
        if (!activeLambda) return false;
        const auto& captures = lambdaCaptures.at(*activeLambda);
        return std::any_of(captures.begin(), captures.end(), [&](const auto* capture) { return capture->symbolIndex == symbol; });
    }

    // LS_KIND of a type; LS_ELEMENT_VALUE for `any`. Types without one cannot become `any`.
    std::optional<std::string_view> TryKindName(TypeId type) const {
        if (type == TypeTable::Any) return "LS_ELEMENT_VALUE";
        if (type == TypeTable::Int || IsEnum(type)) return "LS_KIND_INT";
        if (type == TypeTable::Float) return "LS_KIND_FLOAT";
        if (type == TypeTable::Bool) return "LS_KIND_BOOL";
        if (type == TypeTable::String) return "LS_KIND_STRING";
        if (type == TypeTable::Table) return "LS_KIND_TABLE";
        if (types.IsFunction(type)) return "LS_KIND_CLOSURE";
        if (types.IsList(type)) return "LS_KIND_LIST";
        if (IsObject(type)) return "LS_KIND_OBJECT";
        if (IsStruct(type)) return "LS_KIND_STRUCT";
        return std::nullopt;
    }
    std::string_view KindName(TypeId type) const {
        if (const auto kind = TryKindName(type)) return *kind;
        throw std::runtime_error("Native C backend cannot convert type '" + Name(type) + "' to or from 'any'");
    }
    // A list type gets an LS_TypeInfo ("ls_listtype_<id>") when its elements, nested lists included, have an LS_KIND.
    bool HasListTypeInfo(TypeId list) const {
        assert(types.IsList(list));
        const auto element = types.Element(list);
        return TryKindName(element) && (!types.IsList(element) || HasListTypeInfo(element));
    }
    // The LS_TypeInfo of a class, struct or list type; null for other types.
    void TypeRef(TypeId type) {
        if (IsObject(type) || IsStruct(type)) return Put("&ls_type_", Id{Name(type)});
        if (types.IsList(type) && HasListTypeInfo(type)) return Put("&ls_listtype_", type);
        Put("(const LS_TypeInfo*)0");
    }
    // A C string literal of an attribute name (letters, digits and '_'), or a null pointer when it is empty.
    void CName(std::string_view name) {
        if (name.empty()) return Put("(const char*)0");
        Put('"', name, '"');
    }
    // kind, element_kind, type of an LS_FieldInfo for a slot of this type. A list slot names its element type.
    void FieldDescriptor(TypeId type) {
        if (!types.IsList(type)) return Put(KindName(type), ", 0, ", [&] { TypeRef(type); });
        const auto element = types.Element(type);
        Put(KindName(type), ", ", KindName(element), ", ", [&] { TypeRef(element); });
    }
    void NewList(TypeId element, size_t capacity) { Put("ls_list_create(sizeof(", Type(element), "), ", KindName(element), ", ", [&] { TypeRef(element); }, ", ", capacity, ')'); }
    // True when a value of this user struct holds GC references, directly or in nested structs.
    bool HasReferences(std::string_view structName) {
        if (const auto found = structReferences.find(structName); found != structReferences.end()) return found->second;
        const auto* type = structs.at(std::string(structName));
        const bool result = std::any_of(type->fields.begin(), type->fields.end(), [&](const BoundField& field) {
            return !field.isStatic && (IsManagedObject(field.type) || (IsUserStruct(field.type) && HasReferences(field.type)));
        });
        structReferences.emplace(std::string(structName), result);
        return result;
    }
    bool IsTracedStruct(std::string_view type) { return IsUserStruct(type) && HasReferences(type); }
    // Emits the marking of the references held by a value of `type` stored at `place`, a C lvalue. False when it holds none.
    template <typename Place> bool MarkSlot(size_t depth, std::string_view type, const Place& place) {
        if (type == "any") Line(depth, "ls_mark_value(runtime_context, &", place, ");");
        else if (IsManagedObject(type)) Line(depth, "ls_runtime->mark(runtime_context, (void**)&", place, ");");
        else if (IsTracedStruct(type)) Line(depth, "ls_trace_struct_value(runtime_context, &ls_type_", Id{type}, ", (char*)&", place, ");");
        else return false;
        return true;
    }
    // Module-state slot of a static field: "ls_state.ls_static_<Owner>__<name>".
    auto StaticSlot(std::string_view owner, std::string_view name) { return [this, owner, name] { Put("ls_state.ls_static_", Id{owner}, "__", Id{name}); }; }
    auto StaticSlot(const BoundExpression& value) {
        assert(value.lowering == Lowering::StaticField && value.symbolIndex);
        const auto& symbol = analysis.symbols[*value.symbolIndex];
        return StaticSlot(symbol.owner, symbol.name);
    }

    // Lists and tables are separate native objects, although the language lets either flow into the other.
    void CheckFlow(TypeId target, TypeId source) const {
        if ((types.IsList(target) && source == TypeTable::Table) || (target == TypeTable::Table && types.IsList(source)))
            throw std::runtime_error("Native C backend cannot convert between 'table' and '" + Name(types.IsList(target) ? target : source) + "'");
    }
    const Symbol* ExternalMember(std::string_view owner, std::string_view name) const {
        const auto type = std::find_if(externalTypes.begin(), externalTypes.end(), [&](const auto& value) { return value.name == owner; });
        if (type == externalTypes.end()) return nullptr;
        const auto member = std::find_if(type->members.begin(), type->members.end(), [&](const auto& value) { return value.name == name; });
        return member == type->members.end() ? nullptr : &*member;
    }
    std::vector<TypeId> ParameterTypes(const BoundFunction* function) const {
        std::vector<TypeId> result;
        if (function) for (const auto& parameter : function->parameters) result.push_back(Find(parameter.type));
        return result;
    }
    // External parameters are spelled "Type name", with an optional '?' suffix on the type.
    std::vector<TypeId> ParameterTypes(const Symbol* member) const {
        std::vector<TypeId> result;
        if (member) for (const auto& parameter : member->parameters) result.push_back(Find(SplitParameterLabel(parameter).type));
        return result;
    }

    void Default(TypeId type) {
        if (type == TypeTable::Float) return Put("0.0");
        if (type == TypeTable::String) return Put("ls_intern(\"\")");
        if (type == TypeTable::Table) return Put("ls_table_create()");
        if (type == TypeTable::Any) return Put("ls_nil()");
        if (types.IsFunction(type)) return Put("((LS_Closure*)0)");
        if (types.IsList(type)) return NewList(types.Element(type), 0);
        if (type == TypeTable::Int || type == TypeTable::Bool || IsEnum(type)) return Put('0');
        if (IsStruct(type)) return Put("ls_", Id{Name(type)}, "_default()");
        if (type != TypeTable::Unknown && !IsBuiltinType(Name(type))) return Put("((", Type(type), ")0)");
        throw std::runtime_error("Native C backend cannot default-initialize type '" + Name(type) + "'");
    }

    // Typed C value -> LS_Value.
    template <typename Part> void BoxAs(TypeId type, const Part& part) {
        if (type == TypeTable::Any) return Put(part);
        if (type == TypeTable::Null) return Put("ls_nil()");
        if (type == TypeTable::Int || IsEnum(type)) return Put("ls_box_int(", part, ')');
        if (type == TypeTable::Float) return Put("ls_box_float(", part, ')');
        if (type == TypeTable::Bool) return Put("ls_box_bool(", part, ')');
        if (IsStruct(type)) {
            const auto temp = Temp("box", nextLabel++);
            return Put("({ ", Type(type), ' ', temp, " = ", part, "; ls_box_struct(&ls_type_", Id{Name(type)}, ", &", temp, "); })");
        }
        Put("ls_box_pointer(", KindName(type), ", ", part, ')');
    }
    void Box(const BoundExpression& value, std::string_view owner = {}) { BoxAs(value.type, Emitted(value, owner)); }
    // LS_Value -> typed C value, checked at run time.
    template <typename Part> void Unbox(TypeId type, const Part& part) {
        if (type == TypeTable::Any) return Put(part);
        if (type == TypeTable::Int || IsEnum(type)) return Put("ls_unbox_int(", part, ')');
        if (type == TypeTable::Float) return Put("ls_unbox_float(", part, ')');
        if (type == TypeTable::Bool) return Put("ls_unbox_bool(", part, ')');
        if (types.IsList(type)) return Put("ls_unbox_list(", part, ", ", KindName(types.Element(type)), ", ", [&] { TypeRef(types.Element(type)); }, ')');
        if (IsStruct(type)) {
            const auto temp = Temp("unbox", nextLabel++);
            return Put("({ ", Type(type), ' ', temp, "; ls_unbox_struct(", part, ", &ls_type_", Id{Name(type)}, ", &", temp, "); ", temp, "; })");
        }
        if (IsObject(type)) return Put("((", Type(type), ")ls_unbox_object(", part, ", &ls_type_", Id{Name(type)}, "))");
        Put("((", Type(type), ")ls_unbox_pointer(", part, ", ", KindName(type), "))");
    }
    // Emits value converted to target: boxes into `any`, unboxes out of it.
    void Value(TypeId target, const BoundExpression& value, std::string_view owner) {
        if (target == TypeTable::Unknown || target == value.type) return Expression(value, owner);
        CheckFlow(target, value.type);
        if (target == TypeTable::Any) return Box(value, owner);
        if (value.type == TypeTable::Any) return Unbox(target, Emitted(value, owner));
        if (IsObject(target) && IsObject(value.type)) return Put("((", Type(target), ')', Emitted(value, owner), ')'); // derived to base
        Expression(value, owner);
    }
    // `any` conditions use Lua truthiness.
    void Condition(const BoundExpression& value, std::string_view owner) {
        if (value.type == TypeTable::Any) return Put("ls_value_truthy(", Emitted(value, owner), ')');
        Expression(value, owner);
    }
    // The extern class that declares the field or method a member expression names.
    std::string_view MemberOwner(const BoundExpression& member) const {
        if (member.symbolIndex) return analysis.symbols[*member.symbolIndex].owner;
        return Name(Child(member, 0).type);
    }
    // The extern class that declares the field `x.f` or an implicit `f` names; empty for other fields.
    std::string_view NativeFieldOwner(const BoundExpression& target) const {
        const bool member = target.kind == SyntaxExpression::Kind::Member;
        if (member && IsNative(Child(target, 0).type)) return MemberOwner(target);
        if (!target.symbolIndex || !(member || (target.kind == SyntaxExpression::Kind::Identifier && target.lowering == Lowering::ImplicitField))) return {};
        const auto& symbol = analysis.symbols[*target.symbolIndex];
        return symbol.kind == SymbolKind::Field && !symbol.isStatic && natives.contains(symbol.owner) ? std::string_view(symbol.owner) : std::string_view();
    }
    // The object of a NativeFieldOwner field, as the extern class that declares it.
    auto NativeFieldObject(const BoundExpression& target, std::string_view fieldOwner, std::string_view owner) {
        return [this, &target, fieldOwner, owner] { Put("(LS_", Id{fieldOwner}, "*)"); if (target.kind == SyntaxExpression::Kind::Identifier) Put("self"); else Expression(Child(target, 0), owner); };
    }
    // Generated accessor of an extern field: ls_<Owner>__get_<field> or ls_<Owner>__set_<field>.
    auto Accessor(std::string_view fieldOwner, std::string_view kind, std::string_view field) { return [this, fieldOwner, kind, field] { Put("ls_", Id{fieldOwner}, "__", kind, '_', Id{field}); }; }
    // Arguments start at child `first`, converted to the parameter types where known.
    void Arguments(const std::vector<TypeId>& parameters, const BoundExpression& value, size_t first, bool leadingComma, std::string_view owner) {
        for (size_t i = first; i < value.children.count; ++i) {
            if (i != first || leadingComma) Put(", ");
            const size_t index = i - first;
            if (hoisted.call == &value) { HoistedArgument(index); continue; }
            if (index < parameters.size()) Value(parameters[index], Child(value, i), owner); else Expression(Child(value, i), owner);
        }
    }
    // Writes "LS_Value ls_arguments_<label>[n]; ..." for children [first, count) and returns the array part.
    auto DynamicArguments(const BoundExpression& value, size_t first, std::string_view owner) {
        const size_t count = value.children.count - first;
        const auto label = nextLabel++;
        if (count) {
            Put("LS_Value ls_arguments_", label, '[', count, "]; ");
            for (size_t i{}; i < count; ++i) Put("ls_arguments_", label, '[', i, "] = ", Boxed(Child(value, first + i), owner), "; ");
        }
        return [this, count, label] { if (count) Put("ls_arguments_", label); else Put("(const LS_Value*)0"); };
    }

    // C type of the right operand of `type <op>= right` after conversion.
    void OperandType(TypeId type, Lowering lowering) { if (type == TypeTable::Any) Put(lowering == Lowering::CompoundConcat ? "ls_string" : "LS_Value"); else Put(Type(type)); }
    void Operand(TypeId type, Lowering lowering, const BoundExpression& right, std::string_view owner) {
        if (type != TypeTable::Any) return Value(type, right, owner);
        if (lowering == Lowering::CompoundConcat) return Value(TypeTable::String, right, owner);
        Box(right, owner);
    }
    // left <op> right for compound assignment; right comes from Operand().
    void Combine(TypeId type, char operation, Lowering lowering, const BuiltinOperator* native, const auto& left, const auto& right) {
        if (native) return Put(Id{native->cName}, '(', left, ", ", right, ')');
        if (type == TypeTable::Any) {
            if (lowering == Lowering::CompoundConcat) return Put("ls_box_pointer(LS_KIND_STRING, ls_string_concat((ls_string)ls_unbox_pointer(", left, ", LS_KIND_STRING), ", right, "))");
            return Put("ls_value_arithmetic(", left, ", ", ArithmeticIndex(operation), ", ", right, ')');
        }
        if (type == TypeTable::String && operation == '+') return Put("ls_string_concat(", left, ", ", right, ')');
        if (type == TypeTable::Int) {
            const auto function = operation == '+' ? "ls_int_add" : operation == '-' ? "ls_int_sub" : operation == '*' ? "ls_int_mul" : operation == '%' ? "ls_int_mod" : nullptr;
            if (function) return Put(function, '(', left, ", ", right, ')');
        }
        Put('(', left, ' ', operation, ' ', right, ')');
    }

    // A local, parameter or captured variable. Cells are dereferenced.
    void Variable(size_t symbol, std::string_view name) {
        if (inoutSymbols.contains(symbol)) return Put("(*", [&] { LocalStorage(symbol, name, false); }, ')');
        const bool cell = cellSymbols.contains(symbol);
        if (cell) Put("(*");
        if (CapturedByActive(symbol)) Put("ls_env->", Id{name});
        else LocalStorage(symbol, name, cell);
        if (cell) Put(')');
    }
    // The C variable of a local or parameter: the pointer of a cell; a field of ls_locals in debug builds.
    void LocalStorage(size_t symbol, std::string_view name, bool cell) {
        if (const auto* field = DebugField(symbol)) return Put("ls_locals.", *field);
        if (cell) Put("ls_cell_");
        Put(Id{name});
    }
    // The value a new closure environment stores for a capture: the cell itself for shared captures.
    void CaptureSource(const VariableBinding& binding) {
        if (binding.name == "this") return Put("self");
        if (CapturedByActive(binding.symbolIndex)) return Put("ls_env->", Id{binding.name});
        LocalStorage(binding.symbolIndex, binding.name, IsCell(&binding));
    }
    // Declares a local; a cell local lives on the GC heap.
    void Declare(size_t depth, const VariableBinding* binding, TypeId type, std::string_view name, const auto& initializer) {
        const bool cell = IsCell(binding);
        const auto* field = debugFrame && binding ? &AddDebugLocal(binding->symbolIndex, name, CType(Name(type)) + (cell ? "*" : ""), Name(type), cell) : nullptr;
        const auto storage = [&] { if (field) Put("ls_locals.", *field); else Put(cell ? "ls_cell_" : "", Id{name}); };
        const auto declared = [&] { if (!field) Put(Type(type), cell ? "* " : " "); storage(); };
        if (!cell) return Line(depth, declared, " = ", initializer, ';');
        const auto create = [&] {
            if (IsTracedStruct(Name(type))) Put("ls_alloc(sizeof(", Type(type), "), ls_trace_cell_", Id{Name(type)}, ')');
            else Put("ls_cell_create(sizeof(", Type(type), "), ", KindName(type), ')');
        };
        Line(depth, declared, " = (", Type(type), "*)", create, "; *", storage, " = ", initializer, ';');
    }

    void Expression(const BoundExpression& value, std::string_view owner = {}) {
        const auto child = [&](size_t index) { return Emitted(Child(value, index), owner); };
        const auto& text = value.Text();
        const size_t childCount = value.children.count;
        switch (value.kind) {
        case SyntaxExpression::Kind::Missing: return;
        case SyntaxExpression::Kind::Identifier:
            if (value.lowering == Lowering::Base) return Put("((", Type(value.type), ")self)");
            if (text == "this") return IsUserStruct(owner) ? Put("(*self)") : Put("self");
            if (value.lowering == Lowering::StaticField) return Put(StaticSlot(value));
            if (const auto fieldOwner = NativeFieldOwner(value); !fieldOwner.empty()) return Put(Accessor(fieldOwner, "get", text), '(', NativeFieldObject(value, fieldOwner, owner), ')');
            if (value.lowering == Lowering::ImplicitField) return Put("self->", Id{text});
            if (value.symbolIndex && globalSymbols.contains(*value.symbolIndex)) return Put("ls_state.", Id{text});
            if (value.symbolIndex) return Variable(*value.symbolIndex, text);
            return Put(Id{text});
        case SyntaxExpression::Kind::Number: return Put(text);
        case SyntaxExpression::Kind::String: return Put("ls_intern(", text, ')');
        case SyntaxExpression::Kind::Boolean: return Put(text == "true" ? '1' : '0');
        case SyntaxExpression::Kind::Null: return Put("((void*)0)");
        case SyntaxExpression::Kind::Parenthesized: return Put('(', child(0), ')');
        case SyntaxExpression::Kind::New: return IsInoutCall(value) ? InoutCall(value, owner) : Construction(value, text, 0, owner);
        case SyntaxExpression::Kind::Member: {
            const auto target = Child(value, 0).type;
            if (value.lowering == Lowering::NativeConstant) return Put(value.nativeMember->cName);
            if (value.lowering == Lowering::StaticField) return Put(StaticSlot(value));
            if (types.Kind(target) == TypeKind::Meta) return Put("LS_", Id{types.Target(target)}, '_', Id{text});
            if (IsStruct(target)) return Put('(', child(0), ").", Id{text});
            if (target == TypeTable::Table) return Put("ls_table_get(", child(0), ", ", NameValue(text), ')');
            if (target == TypeTable::Any) return Put("ls_value_get(", child(0), ", ", NameValue(text), ')');
            if (const auto fieldOwner = NativeFieldOwner(value); !fieldOwner.empty()) return Put(Accessor(fieldOwner, "get", text), '(', NativeFieldObject(value, fieldOwner, owner), ')');
            if (externalNames.contains(Name(target))) if (const auto* symbol = ExternalMember(Name(target), text); symbol && symbol->kind == SymbolKind::Field)
                throw std::runtime_error("Native C backend cannot directly access field '" + text + "' on opaque external type '" + Name(target) + "'");
            return Put(child(0), "->", Id{text});
        }
        case SyntaxExpression::Kind::Call: return IsInoutCall(value) ? InoutCall(value, owner) : Call(value, owner);
        case SyntaxExpression::Kind::Inout: throw std::runtime_error("Native C backend reached an inout argument outside its call");
        case SyntaxExpression::Kind::Unary:
            if (value.nativeOperator) return Put(Id{value.nativeOperator->cName}, '(', child(0), ')');
            if (Child(value, 0).type == TypeTable::Any) return Put(text == "!" ? "(!ls_value_truthy(" : "ls_value_negate(", child(0), text == "!" ? "))" : ")");
            if (text == "-" && value.type == TypeTable::Int) return Put("ls_int_neg(", child(0), ')');
            return Put('(', text, child(0), ')');
        case SyntaxExpression::Kind::Binary: {
            if (value.lowering == Lowering::Concat) return Put("ls_string_concat(", child(0), ", ", child(1), ')');
            if (value.nativeOperator) return Put(Id{value.nativeOperator->cName}, '(', child(0), ", ", child(1), ')');
            const auto left = Child(value, 0).type, right = Child(value, 1).type;
            if ((text == "==" || text == "!=") && (IsNativeBacked(left) || IsNativeBacked(right)) && left != TypeTable::Any && right != TypeTable::Any)
                return Put(text == "!=" ? "(!" : "(", "ls_native_same(", child(0), ", ", child(1), "))");
            if ((text == "==" || text == "!=") && IsStruct(left) && left == right) return Put(text == "!=" ? "(!" : "(", "ls_", Id{Name(left)}, "_eq(", child(0), ", ", child(1), "))");
            if (left == TypeTable::Any || right == TypeTable::Any) {
                if (text == "&&" || text == "||")
                    return Put('(', [&] { Condition(Child(value, 0), owner); }, ' ', text, ' ', [&] { Condition(Child(value, 1), owner); }, ')');
                const auto boxedLeft = Boxed(Child(value, 0), owner), boxedRight = Boxed(Child(value, 1), owner);
                if (text == "==" || text == "!=") return Put(text == "!=" ? "(!" : "(", "ls_value_equal(", boxedLeft, ", ", boxedRight, "))");
                if (text == "<" || text == ">" || text == "<=" || text == ">=") {
                    const int operation = text == "<" ? 0 : text == "<=" ? 1 : text == ">" ? 2 : 3;
                    return Put("ls_value_compare(", boxedLeft, ", ", operation, ", ", boxedRight, ')');
                }
                if (text.size() == 1) return Put("ls_value_arithmetic(", boxedLeft, ", ", ArithmeticIndex(text.front()), ", ", boxedRight, ')');
            }
            if ((text == "==" || text == "!=") && left == TypeTable::String)
                return Put(text == "!=" ? "!" : "", "ls_string_equal(", child(0), ", ", child(1), ')');
            if ((text == "==" || text == "!=") && IsClass(left) && IsClass(right) && left != right)
                return Put("((void*)", child(0), ' ', text, " (void*)", child(1), ')');
            if (left == TypeTable::Int && right == TypeTable::Int) {
                const char* function = text == "+" ? "ls_int_add" : text == "-" ? "ls_int_sub" : text == "*" ? "ls_int_mul" : text == "/" ? "ls_int_div" : text == "%" ? "ls_int_mod" : nullptr;
                if (function) return Put(function, '(', child(0), ", ", child(1), ')');
            }
            return Put('(', child(0), ' ', text, ' ', child(1), ')');
        }
        case SyntaxExpression::Kind::Assignment: return Assignment(value, owner);
        case SyntaxExpression::Kind::Table: {
            if (types.IsList(value.type)) {
                const auto element = types.Element(value.type);
                if (!childCount) return NewList(element, 0);
                const auto literal = Temp("literal", nextLabel++);
                Put("({ LS_List* ", literal, " = "); NewList(element, childCount); Put("; ");
                for (size_t i{}; i < childCount; ++i) Put("*(", Type(element), "*)ls_list_push(", literal, ") = ", Converted(element, Child(value, i), owner), "; ");
                return Put(literal, "; })");
            }
            if (!childCount) return Put("ls_table_create()");
            const auto literal = Temp("literal", nextLabel++);
            Put("({ ls_table ", literal, " = ls_table_create(); ");
            for (size_t i{}; i < childCount; ++i) {
                const auto& entry = Child(value, i);
                Put("ls_table_set(", literal, ", ");
                if (entry.kind == SyntaxExpression::Kind::TableEntry && !entry.Text().empty()) Put(NameValue(entry.Text()), ", ", Boxed(Child(entry, 0), owner));
                else if (entry.kind == SyntaxExpression::Kind::TableEntry) Put(Boxed(Child(entry, 0), owner), ", ", Boxed(Child(entry, 1), owner));
                else Put("ls_box_int(", i + 1, "), ", Boxed(entry, owner));
                Put("); ");
            }
            return Put(literal, "; })");
        }
        case SyntaxExpression::Kind::Index: {
            const auto target = Child(value, 0).type;
            if (value.lowering == Lowering::ListIndex) return Put("(*(", Type(value.type), "*)ls_list_at(", child(0), ", ", Converted(TypeTable::Int, Child(value, 1), owner), "))");
            if (target == TypeTable::Table) return Put("ls_table_get(", child(0), ", ", Boxed(Child(value, 1), owner), ')');
            if (target == TypeTable::Any) return Put("ls_value_get(", child(0), ", ", Boxed(Child(value, 1), owner), ')');
            throw std::runtime_error("Native C backend does not support indexing type '" + Name(target) + "'");
        }
        case SyntaxExpression::Kind::Lambda: {
            const auto found = lambdaIndices.find(&value);
            if (found == lambdaIndices.end()) throw std::runtime_error("Native closure metadata was not collected");
            const auto index = found->second;
            const auto arity = analysis.syntax.Parameters(*value.syntax).size();
            if (lambdaCaptures[index].empty()) return Put("ls_closure_create(ls_lambda_", index, ", (void*)0, ", arity, ')');
            const auto environment = Temp("environment", nextLabel++);
            Put("({ LS_LambdaEnv_", index, "* ", environment, " = (LS_LambdaEnv_", index, "*)ls_alloc(sizeof(LS_LambdaEnv_", index, "), ls_trace_lambda_env_", index, "); ");
            for (const auto* capture : lambdaCaptures[index]) Put(environment, "->", Id{capture->name}, " = ", [&] { CaptureSource(*capture); }, "; ");
            return Put("ls_closure_create(ls_lambda_", index, ", ", environment, ", ", arity, "); })");
        }
        case SyntaxExpression::Kind::Slice: return Put("ls_list_slice(", child(0), ", ", child(1), ", ", child(2), ')');
        case SyntaxExpression::Kind::Is: case SyntaxExpression::Kind::As: return TypeTest(value, owner);
        case SyntaxExpression::Kind::TypeOf: {
            // The host finds the type info the first time; later reads use the slot. A destroyed type info is found again.
            assert(typeOfProvider);
            const auto slot = [&] { Put("ls_typeof_", Id{text}); };
            return Put("(ls_object_alive(", slot, ") ? ", slot, " : (", slot, " = ", Fn{typeOfProvider->owner, typeOfProvider->name}, "(ls_intern(\"", text, "\"))))");
        }
        case SyntaxExpression::Kind::TableEntry: case SyntaxExpression::Kind::Range:
            throw std::runtime_error("Native C backend does not support this expression kind yet (type '" + Name(value.type) + "')");
        }
        throw std::runtime_error("Native C backend encountered an unknown expression");
    }

    // `x is T` / `x as T`. A class operand tests its LS_TypeInfo chain; an `any` operand also tests its kind.
    void TypeTest(const BoundExpression& value, std::string_view owner) {
        const bool is = value.kind == SyntaxExpression::Kind::Is;
        const auto& operand = Child(value, 0);
        const auto target = Find(value.Text());
        if (operand.type != TypeTable::Any) {
            if (!IsObject(target)) throw std::runtime_error("Native C backend cannot test for external type '" + Name(target) + "'");
            if (IsSubclass(Name(operand.type), Name(target))) return is ? Put("ls_object_alive(", Emitted(operand, owner), ')') : Put("((", Type(target), ')', Emitted(operand, owner), ')');
            if (is) return Put("ls_object_is(", Emitted(operand, owner), ", &ls_type_", Id{Name(target)}, ')');
            return Put("((", Type(target), ")ls_object_as(", Emitted(operand, owner), ", &ls_type_", Id{Name(target)}, "))");
        }
        // kind, element_kind and type for ls_value_is; a list names its element type.
        const auto test = [&](const auto& subject) {
            Put("ls_value_is(", subject, ", ", KindName(target), ", ");
            if (types.IsList(target)) Put(KindName(types.Element(target)), ", ", [&] { TypeRef(types.Element(target)); });
            else Put("0, ", [&] { TypeRef(target); });
            Put(')');
        };
        if (is) return test(Emitted(operand, owner));
        const auto temp = Temp("as", nextLabel++);
        Put("({ LS_Value ", temp, " = ", Emitted(operand, owner), "; (", Type(target), ")(", [&] { test(temp); }, " ? ", temp, ".as.pointer : (void*)0); })");
    }

    void Assignment(const BoundExpression& value, std::string_view owner) {
        const auto& text = value.Text();
        const auto& target = Child(value, 0);
        const auto& right = Child(value, 1);
        const bool compound = text != "=";
        assert(!compound || text.size() == 2);
        const char operation = text.front();
        const auto label = nextLabel++;
        const auto update = Temp("value", label);
        // table[key], table.name, and the same on `any`: evaluates container, key, then value.
        if ((target.kind == SyntaxExpression::Kind::Index || target.kind == SyntaxExpression::Kind::Member) && IsDynamic(Child(target, 0).type)) {
            const bool table = Child(target, 0).type == TypeTable::Table;
            const auto container = Temp("container", label), key = Temp("key", label);
            Put("({ ", table ? "ls_table " : "LS_Value ", container, " = ", Emitted(Child(target, 0), owner), "; LS_Value ", key, " = ");
            if (target.kind == SyntaxExpression::Kind::Index) Box(Child(target, 1), owner); else Put(NameValue(target.Text()));
            Put("; LS_Value ", update, " = ");
            if (!compound) Box(right, owner);
            else Combine(TypeTable::Any, operation, value.lowering, nullptr, [&] { Put(table ? "ls_table_get(" : "ls_value_get(", container, ", ", key, ')'); }, [&] { Operand(TypeTable::Any, value.lowering, right, owner); });
            return Put("; ", table ? "ls_table_set(" : "ls_value_set(", container, ", ", key, ", ", update, "); ", update, "; })");
        }
        const auto type = target.type;
        // An extern field: the setter returns the stored value. Evaluates the object, then the value.
        if (const auto fieldOwner = NativeFieldOwner(target); !fieldOwner.empty()) {
            const auto object = Temp("object", label);
            Put("({ LS_", Id{fieldOwner}, "* ", object, " = ", NativeFieldObject(target, fieldOwner, owner), "; ", Accessor(fieldOwner, "set", target.Text()), '(', object, ", ");
            if (!compound) Value(type, right, owner);
            else Combine(type, operation, value.lowering, value.nativeOperator, [&] { Put(Accessor(fieldOwner, "get", target.Text()), '(', object, ')'); }, [&] { Operand(type, value.lowering, right, owner); });
            return Put("); })");
        }
        const auto slot = Temp("slot", label);
        const auto operand = [&] { Operand(type, value.lowering, right, owner); };
        const auto store = [&] { if (compound) Combine(type, operation, value.lowering, value.nativeOperator, [&] { Put('*', slot); }, update); else Put(update); };
        if (value.lowering == Lowering::ListSet) {
            const auto list = Temp("list", label), index = Temp("index", label);
            // Evaluates list, index, then value; the slot address is taken last because the value may grow the list.
            Put("({ LS_List* ", list, " = ", Emitted(Child(target, 0), owner), "; ls_int ", index, " = ", Converted(TypeTable::Int, Child(target, 1), owner), "; ");
            Put([&] { OperandType(type, value.lowering); }, ' ', update, " = ", operand, "; ");
            return Put(Type(type), "* ", slot, " = (", Type(type), "*)ls_list_at(", list, ", ", index, "); *", slot, " = ", store, "; })");
        }
        if (!compound) return Put('(', Emitted(target, owner), " = ", Converted(type, right, owner), ')');
        if (target.kind == SyntaxExpression::Kind::Identifier) return Put('(', Emitted(target, owner), " = ", [&] { Combine(type, operation, value.lowering, value.nativeOperator, Emitted(target, owner), operand); }, ')');
        Put("({ ", Type(type), "* ", slot, " = &(", Emitted(target, owner), "); ", [&] { OperandType(type, value.lowering); }, ' ', update, " = ", operand, "; *", slot, " = ", store, "; })");
    }

    void Call(const BoundExpression& value, std::string_view owner) {
        assert(value.kind == SyntaxExpression::Kind::Call && value.children.count >= 1);
        const auto& callee = Child(value, 0);
        const size_t childCount = value.children.count;
        const auto argument = [&](size_t index) { return Emitted(Child(value, index + 1), owner); };
        const auto converted = [&](size_t index, TypeId type) { return Converted(type, Child(value, index + 1), owner); };
        if (value.lowering == Lowering::Printf) {
            Put("({ ");
            const auto arguments = DynamicArguments(value, 2, owner);
            return Put("ls_printf(", argument(0), ", ", childCount - 2, ", ", arguments, "); })");
        }
        if (types.IsFunction(callee.type)) {
            Put("({ ");
            const auto arguments = DynamicArguments(value, 1, owner);
            const auto call = [&] { Put("ls_closure_call(", Emitted(callee, owner), ", ", arguments, ')'); };
            if (value.type == TypeTable::Void) return Put(call, "; })");
            return Put([&] { Unbox(value.type, call); }, "; })");
        }
        const auto receiver = [&] { Expression(Child(callee, 0), owner); };
        switch (value.lowering) {
        case Lowering::CastIdentity: case Lowering::CastIntFloat: case Lowering::CastNumberBool: return Put('(', Type(value.type), ")(", argument(0), ')');
        case Lowering::CastBoolNumber: return Put('(', argument(0), " != 0)");
        case Lowering::CastBoolString: return Put("ls_string_truthy(", argument(0), ')');
        case Lowering::CastNumberString: return Put(value.type == TypeTable::Int ? "ls_string_to_int(" : "ls_string_to_float(", argument(0), ')');
        case Lowering::CastString: return Put("ls_to_string(", Boxed(Child(value, 1), owner), ')');
        case Lowering::CastDynamic:
            if (value.type == TypeTable::Bool) return Put("ls_value_truthy(", argument(0), ')');
            return Unbox(value.type, argument(0));
        case Lowering::StringLength: return Put("ls_string_length(", receiver, ')');
        case Lowering::StringContains: return Put("ls_string_contains(", receiver, ", ", converted(0, TypeTable::String), ')');
        case Lowering::StringStartsWith: return Put("ls_string_starts_with(", receiver, ", ", converted(0, TypeTable::String), ')');
        case Lowering::StringEndsWith: return Put("ls_string_ends_with(", receiver, ", ", converted(0, TypeTable::String), ')');
        case Lowering::StringSubstring:
            if (childCount == 3) return Put("ls_string_substring(", receiver, ", ", converted(0, TypeTable::Int), ", ", converted(1, TypeTable::Int), ", 1)");
            return Put("ls_string_substring(", receiver, ", ", converted(0, TypeTable::Int), ", 0, 0)");
        case Lowering::StringLower: return Put("ls_string_case(", receiver, ", 0)");
        case Lowering::StringUpper: return Put("ls_string_case(", receiver, ", 1)");
        case Lowering::ListPushBack: {
            const auto element = types.Element(Child(callee, 0).type);
            const auto label = nextLabel++;
            const auto list = Temp("list", label), pushed = Temp("value", label);
            return Put("({ LS_List* ", list, " = ", receiver, "; ", Type(element), ' ', pushed, " = ", converted(0, element), "; *(", Type(element), "*)ls_list_push(", list, ") = ", pushed, "; })");
        }
        case Lowering::ListPopBack: return Put("(*(", Type(value.type), "*)ls_list_pop(", receiver, "))");
        case Lowering::ListSize: return Put("ls_list_size(", receiver, ')');
        case Lowering::ListEmpty: return Put("(ls_list_size(", receiver, ") == 0)");
        case Lowering::ListClear: return Put("ls_list_clear(", receiver, ')');
        default: break;
        }
        // Lua receiver:name(...) on a table or `any` value.
        if (callee.kind == SyntaxExpression::Kind::Member && IsDynamic(Child(callee, 0).type)) {
            Put("({ ");
            const auto arguments = DynamicArguments(value, 1, owner);
            return Put("ls_value_call_member(", Boxed(Child(callee, 0), owner), ", LS_NAME(", Id{callee.Text()}, "), ", arguments, ", ", childCount - 1, "); })");
        }
        if (callee.type == TypeTable::Any) {
            Put("({ ");
            const auto arguments = DynamicArguments(value, 1, owner);
            return Put("ls_value_call(", Emitted(callee, owner), ", ", arguments, ", ", childCount - 1, "); })");
        }
        if (value.lowering == Lowering::Construct) return Construction(value, Name(value.type), 1, owner);
        if (value.lowering == Lowering::NativeStatic) {
            Put(Id{value.nativeMember->cName}, '('); NativeArguments(*value.nativeMember, value, 1, false, owner);
            return Put(')');
        }
        if (value.lowering == Lowering::NativeMethod) {
            Put(Id{value.nativeMember->cName}, '(', receiver); NativeArguments(*value.nativeMember, value, 1, true, owner);
            return Put(')');
        }
        if (const auto [native, member] = NativeBaseCall(value); native) {
            Put(Fn{native->name, BaseThunkName(member->name)}, "((LS_", Id{native->name}, "*)self");
            Arguments(ParameterTypes(member), value, 1, true, owner);
            return Put(')');
        }
        if (const auto* member = NativeMember(value)) {
            // ls_<Owner>_<name> checks the receiver and calls the host thunk.
            const auto call = [&] {
                Put(Fn{member->owner, member->name}, '(');
                if (!member->isStatic) Put("(LS_", Id{member->owner}, "*)", [&] { if (callee.lowering == Lowering::ImplicitMethod) Put("self"); else Expression(Child(callee, 0), owner); });
                Arguments(ParameterTypes(member), value, 1, !member->isStatic, owner);
                Put(')');
            };
            // A generic method returns its constraint; the call has the type of its type argument.
            if (!member->typeParameter.empty() && value.type != Find(member->type)) return Put("((", Type(value.type), ")ls_object_as(", call, ", &ls_type_", Id{Name(value.type)}, "))");
            return call();
        }
        if (const auto* ref = FindFunctionRef(value)) return UserCall(value, callee, *ref, owner);
        if (callee.kind == SyntaxExpression::Kind::Member) {
            const auto& target = Child(callee, 0);
            const auto& type = Name(target.type);
            if (classes.contains(type) || structs.contains(type)) throw std::runtime_error("Native C backend cannot call unknown method '" + callee.Text() + "' on '" + type + "'");
            if (const auto* member = ExternalMember(type, callee.Text()); member && std::any_of(member->parameters.begin(), member->parameters.end(), [](const std::string& label) { return SplitParameterLabel(label).isInout; }))
                throw std::runtime_error("Native C backend cannot pass inout arguments to external method '" + type + "." + callee.Text() + "'");
            Put(Fn{type, callee.Text()}, '(', Emitted(target, owner));
            Arguments(ParameterTypes(ExternalMember(type, callee.Text())), value, 1, true, owner);
        } else {
            Put(Emitted(callee, owner), '(');
            Arguments({}, value, 1, false, owner);
        }
        Put(')');
    }

    // The extern method or constructor a call or `new` resolved to.
    const Symbol* NativeMember(const BoundExpression& value) const {
        if (!value.symbolIndex) return nullptr;
        const auto& symbol = analysis.symbols[*value.symbolIndex];
        return symbol.kind == SymbolKind::Method && natives.contains(symbol.owner) ? &symbol : nullptr;
    }
    // Native arguments from child `first`; omitted optional parameters use their default C expression.
    void NativeArguments(const BuiltinMember& member, const BoundExpression& value, size_t first, bool leadingComma, std::string_view owner) {
        const size_t provided = value.children.count - first;
        for (size_t i{}; i < member.parameters.size(); ++i) {
            if (i || leadingComma) Put(", ");
            if (i < provided) Value(Find(member.parameters[i].type), Child(value, first + i), owner); else Put(member.parameters[i].defaultValue);
        }
    }

    // `new T(...)`, or `T(...)` for a struct. Arguments start at child `first`.
    void Construction(const BoundExpression& value, std::string_view typeName, size_t first, std::string_view owner) {
        if (value.nativeMember) {
            Put(Id{value.nativeMember->cName}, '('); NativeArguments(*value.nativeMember, value, first, false, owner);
            return Put(')');
        }
        const auto* ref = FindFunctionRef(value);
        if (!ref && IsUserStruct(typeName)) return Put("ls_", Id{typeName}, "_default()");
        if (ref) Put(ref->cName); else Put(Fn{typeName, "new"});
        Put('(');
        Arguments(ref ? ParameterTypes(ref->function) : ParameterTypes(NativeMember(value)), value, first, false, owner);
        Put(')');
    }

    // Storage of an inout argument. Hoisting evaluates its containers once; the write finds the slot again,
    // because the call can grow a list that holds it.
    struct InoutPlace {
        enum class Kind { Direct, ListElement, ObjectField, Accessor } kind{};
        const BoundExpression* target{};
        const BoundExpression* root{}; // target without its struct members and parentheses
        std::string_view fieldOwner;   // Accessor: the extern class of the field
        size_t label{}, index{};
    };
    auto ArgumentTemp(std::string_view name, size_t label, size_t index) { return [this, name, label, index] { Put("ls_", name, '_', label, '_', index); }; }
    auto PlaceTemp(const InoutPlace& place, std::string_view name) { return ArgumentTemp(name, place.label, place.index); }
    const BoundExpression& Unparenthesized(const BoundExpression& value) const { return value.kind == SyntaxExpression::Kind::Parenthesized ? Unparenthesized(Child(value, 0)) : value; }
    // The module function a call or `new` runs when it has inout parameters; null otherwise.
    const BoundFunction* InoutFunction(const BoundExpression& call) const {
        if (call.kind != SyntaxExpression::Kind::Call && call.kind != SyntaxExpression::Kind::New) return nullptr;
        const auto* ref = FindFunctionRef(call);
        if (!ref) return nullptr;
        const auto& parameters = ref->function->parameters;
        return std::any_of(parameters.begin(), parameters.end(), [](const SyntaxParameter& parameter) { return parameter.isInout; }) ? ref->function : nullptr;
    }
    bool IsInoutCall(const BoundExpression& call) const { return InoutFunction(call) != nullptr; }
    static size_t FirstArgument(const BoundExpression& call) { return call.kind == SyntaxExpression::Kind::New ? 0 : 1; }
    // The storage passed for inout parameter `index`; the `inout` mark on the argument is optional.
    const BoundExpression& InoutTarget(const BoundExpression& call, size_t index) const {
        const auto& argument = Child(call, FirstArgument(call) + index);
        return argument.kind == SyntaxExpression::Kind::Inout ? Child(argument, 0) : argument;
    }
    InoutPlace HoistPlace(const BoundExpression& target, size_t label, size_t index, std::string_view owner) {
        InoutPlace place{InoutPlace::Kind::Direct, &target, &target, NativeFieldOwner(target), label, index};
        if (!place.fieldOwner.empty()) {
            place.kind = InoutPlace::Kind::Accessor;
            Put("LS_", Id{place.fieldOwner}, "* ", PlaceTemp(place, "object"), " = ", NativeFieldObject(target, place.fieldOwner, owner), "; ");
            return place;
        }
        const auto isStructMember = [&](const BoundExpression& value) { return value.kind == SyntaxExpression::Kind::Member && IsStruct(Child(value, 0).type); };
        while (place.root->kind == SyntaxExpression::Kind::Parenthesized || isStructMember(*place.root)) place.root = &Child(*place.root, 0);
        const auto& root = *place.root;
        if (root.kind == SyntaxExpression::Kind::Index) {
            assert(root.lowering == Lowering::ListIndex);
            place.kind = InoutPlace::Kind::ListElement;
            Put("LS_List* ", PlaceTemp(place, "list"), " = ", Emitted(Child(root, 0), owner), "; ls_int ", PlaceTemp(place, "index"), " = ", Converted(TypeTable::Int, Child(root, 1), owner), "; ");
        } else if (root.kind == SyntaxExpression::Kind::Member && root.lowering != Lowering::StaticField) {
            assert(IsObject(Child(root, 0).type));
            place.kind = InoutPlace::Kind::ObjectField;
            Put(Type(Child(root, 0).type), ' ', PlaceTemp(place, "object"), " = ", Emitted(Child(root, 0), owner), "; ");
        }
        return place;
    }
    // The target as a C lvalue, with its root read from the hoisted temps.
    void PlaceLvalue(const InoutPlace& place, const BoundExpression& node, std::string_view owner) {
        if (&node == place.root) {
            switch (place.kind) {
            case InoutPlace::Kind::ListElement: return Put("(*(", Type(node.type), "*)ls_list_at(", PlaceTemp(place, "list"), ", ", PlaceTemp(place, "index"), "))");
            case InoutPlace::Kind::ObjectField: return Put(PlaceTemp(place, "object"), "->", Id{node.Text()});
            default: return Expression(node, owner);
            }
        }
        if (node.kind == SyntaxExpression::Kind::Parenthesized) return PlaceLvalue(place, Child(node, 0), owner);
        Put('(', [&] { PlaceLvalue(place, Child(node, 0), owner); }, ")." , Id{node.Text()});
    }
    void ReadPlace(const InoutPlace& place, std::string_view owner) {
        if (place.kind == InoutPlace::Kind::Accessor) return Put(Accessor(place.fieldOwner, "get", place.target->Text()), '(', PlaceTemp(place, "object"), ')');
        PlaceLvalue(place, *place.target, owner);
    }
    template <typename Part> void WritePlace(const InoutPlace& place, const Part& value, std::string_view owner) {
        if (place.kind == InoutPlace::Kind::Accessor) return Put(Accessor(place.fieldOwner, "set", place.target->Text()), '(', PlaceTemp(place, "object"), ", ", value, ')');
        Put([&] { PlaceLvalue(place, *place.target, owner); }, " = ", value);
    }
    // Copy-in, copy-out: evaluates every argument in order into ls_argument_<label>_<i>, passes each inout copy
    // by address, then writes the copies back in argument order.
    void InoutCall(const BoundExpression& value, std::string_view owner) {
        const auto* function = InoutFunction(value);
        assert(function);
        const bool constructing = value.kind == SyntaxExpression::Kind::New;
        const size_t first = FirstArgument(value);
        const auto parameters = ParameterTypes(function);
        assert(value.children.count - first == parameters.size());
        const auto label = nextLabel++;
        HoistedArguments arguments{&value, label, {}};
        std::vector<std::optional<InoutPlace>> places;
        Put("({ ");
        for (size_t i{}; i < parameters.size(); ++i) {
            const auto& argument = Child(value, first + i);
            const bool inout = function->parameters[i].isInout;
            assert(inout || argument.kind != SyntaxExpression::Kind::Inout);
            arguments.inout.push_back(inout);
            places.push_back(inout ? std::optional(HoistPlace(InoutTarget(value, i), label, i, owner)) : std::nullopt);
            Put(Type(parameters[i]), ' ', ArgumentTemp("argument", label, i), " = ");
            if (inout) ReadPlace(*places.back(), owner); else Value(parameters[i], argument, owner);
            Put("; ");
        }
        const bool returns = value.type != TypeTable::Void;
        const auto result = Temp("result", label);
        const auto saved = std::exchange(hoisted, std::move(arguments));
        if (returns) Put(Type(value.type), ' ', result, " = ");
        if (constructing) Construction(value, value.Text(), 0, owner); else Call(value, owner);
        Put("; ");
        hoisted = saved;
        for (size_t i{}; i < places.size(); ++i) if (places[i]) { WritePlace(*places[i], ArgumentTemp("argument", label, i), owner); Put("; "); }
        if (returns) Put(result); else Put("(void)0");
        Put("; })");
    }
    void HoistedArgument(size_t index) {
        assert(index < hoisted.inout.size());
        if (hoisted.inout[index]) Put('&');
        Put(ArgumentTemp("argument", hoisted.label, index));
    }

    // Calls a function declared in this module: instance (receiver first), static, or implicit-this.
    void UserCall(const BoundExpression& value, const BoundExpression& callee, const FunctionRef& ref, std::string_view owner) {
        const auto& function = *ref.function;
        const auto parameters = ParameterTypes(ref.function);
        if (function.isStatic) {
            Put(ref.cName, '('); Arguments(parameters, value, 1, false, owner);
            return Put(')');
        }
        const bool implicitThis = callee.lowering == Lowering::ImplicitMethod;
        const BoundExpression* target = implicitThis ? nullptr : &Child(callee, 0);
        if (!target || !IsUserStruct(Name(target->type))) {
            const auto receiver = [&] { if (target) Expression(*target, owner); else Put("self"); };
            const std::string_view receiverClass = target ? std::string_view(Name(target->type)) : owner;
            const auto slot = slotOfSymbol.find(function.symbolIndex);
            if (slot == slotOfSymbol.end() || (target && target->lowering == Lowering::Base)) return DirectCall(ref, receiverClass, receiver, value, owner);
            const auto found = classes.find(receiverClass);
            assert(found != classes.end());
            const auto* type = found->second;
            const auto& entry = vtables.at(type).at(slot->second);
            // Nothing can override a sealed method, or anything in a sealed class.
            if (entry.impl && (type->isSealed || analysis.symbols[entry.impl->symbolIndex].isSealed)) return DirectCall(RefOf(*entry.impl), receiverClass, receiver, value, owner);
            const Id introducer{entry.introducer->name};
            const auto temp = Temp("receiver", nextLabel++);
            Put("({ LS_", introducer, "* ", temp, " = (LS_", introducer, "*)", receiver, "; ((", CTypeName{function.returnType}, " (*)(LS_", introducer, '*');
            for (const auto& parameter : function.parameters) Put(", ", CTypeName{parameter.type}, parameter.isInout ? "*" : "");
            Put("))", temp, "->ls_type->vtable[", slot->second, "])(", temp); Arguments(parameters, value, 1, true, owner);
            return Put("); })");
        }
        const auto& structTarget = *target;
        // Struct methods take the receiver by pointer; a temporary receiver is copied into a local first.
        if (IsAddressable(structTarget)) {
            Put(ref.cName, "(&(", Emitted(structTarget, owner), ")"); Arguments(parameters, value, 1, true, owner);
            return Put(')');
        }
        const auto temp = Temp("receiver", nextLabel++);
        Put("({ ", Type(structTarget.type), ' ', temp, " = ", Emitted(structTarget, owner), "; ", ref.cName, "(&", temp); Arguments(parameters, value, 1, true, owner);
        Put("); })");
    }
    // A call without dispatch. An inherited method takes its declaring class, so the receiver is cast.
    void DirectCall(const FunctionRef& ref, std::string_view receiverClass, const auto& receiver, const BoundExpression& value, std::string_view owner) {
        Put(ref.cName, '(');
        if (receiverClass != ref.owner->name) Put("(LS_", Id{ref.owner->name}, "*)");
        Put(receiver); Arguments(ParameterTypes(ref.function), value, 1, true, owner);
        Put(')');
    }

    // A block: its locals go out of scope after it.
    void Statements(NodeRange statements, size_t depth, std::string_view owner = {}, std::optional<size_t> continueLabel = {}, bool moduleTopLevel = false) {
        const auto scope = DebugScope();
        StatementList(statements, depth, owner, continueLabel, moduleTopLevel);
        CloseDebugScope(scope);
    }
    void StatementList(NodeRange statements, size_t depth, std::string_view owner, std::optional<size_t> continueLabel, bool moduleTopLevel) {
        for (const auto& statement : program.Statements(statements)) {
            const auto& expression = program.Expression(statement.expression);
            const auto condition = [&] { Condition(expression, owner); };
            const size_t offset = statement.syntax->span.offset;
            // Loops run the hook in their condition.
            if (statement.kind != SyntaxStatement::Kind::While && statement.kind != SyntaxStatement::Kind::For) DebugStatement(depth, offset);
            switch (statement.kind) {
            case SyntaxStatement::Kind::Local:
            {
                const auto initializer = [&] { if (expression.kind == SyntaxExpression::Kind::Missing) Default(statement.type); else Value(statement.type, expression, owner); };
                if (moduleTopLevel) Line(depth, "ls_state.", Id{statement.Name()}, " = ", initializer, ';');
                else Declare(depth, BindingAt(statement.syntax->nameSpan.offset), statement.type, statement.Name(), initializer);
                break;
            }
            case SyntaxStatement::Kind::Expression: Line(depth, Emitted(expression, owner), ';'); break;
            case SyntaxStatement::Kind::Return:
                if (expression.kind == SyntaxExpression::Kind::Missing) { if (constructorResult.empty()) Line(depth, "return;"); else Line(depth, "return ", constructorResult, ';'); }
                else if (activeLambda) Line(depth, "*result = ", Boxed(expression, owner), "; return;");
                else Line(depth, "return ", Converted(functionReturn, expression, owner), ';');
                break;
            case SyntaxStatement::Kind::Break: Line(depth, "break;"); break;
            case SyntaxStatement::Kind::Continue:
                if (continueLabel) Line(depth, "goto ls_continue_", *continueLabel, ';');
                else Line(depth, "continue;");
                break;
            case SyntaxStatement::Kind::If:
                Line(depth, "if (", condition, ") {"); Statements(statement.body, depth + 1, owner, continueLabel); Line(depth, '}');
                if (statement.elseBody.count) { Line(depth, "else {"); Statements(statement.elseBody, depth + 1, owner, continueLabel); Line(depth, '}'); }
                break;
            case SyntaxStatement::Kind::While:
                LineDirective(offset);
                Line(depth, "while (", LoopCondition(offset, condition), ") {"); Statements(statement.body, depth + 1, owner); Line(depth, '}'); break;
            case SyntaxStatement::Kind::For:
            {
                const auto label = nextLabel++;
                const auto& increment = program.Expression(statement.increment);
                const auto scope = DebugScope(); // the initializer's locals
                Line(depth, '{'); StatementList(statement.initializer, depth + 1, owner, {}, false);
                LineDirective(offset);
                Line(depth + 1, "while (", LoopCondition(offset, condition), ") {");
                Statements(statement.body, depth + 2, owner, label);
                Line(depth + 2, "ls_continue_", label, ":;");
                if (increment.kind != SyntaxExpression::Kind::Missing) Line(depth + 2, Emitted(increment, owner), ';');
                Line(depth + 1, '}'); Line(depth, '}');
                CloseDebugScope(scope);
                break;
            }
            case SyntaxStatement::Kind::ForEach: ForEach(statement, depth, owner); break;
            }
        }
    }

    void ForEach(const BoundStatement& statement, size_t depth, std::string_view owner) {
        const auto& iterable = program.Expression(statement.expression);
        const auto label = nextLabel++;
        const auto& name = statement.Name();
        const auto& secondName = statement.SecondName();
        const auto* first = BindingAt(statement.syntax->nameSpan.offset);
        const auto* second = secondName.empty() ? nullptr : BindingAt(statement.syntax->secondNameSpan.offset);
        const auto scope = DebugScope(); // the loop variables
        Line(depth, '{');
        if (iterable.kind == SyntaxExpression::Kind::Range) {
            // Inclusive bounds, evaluated once; the counter is separate so the body cannot change the iteration.
            const auto counter = Temp("counter", label), last = Temp("last", label);
            Line(depth + 1, "ls_int ", counter, " = ", Emitted(Child(iterable, 0), owner), ", ", last, " = ", Emitted(Child(iterable, 1), owner), ';');
            Line(depth + 1, "if (", counter, " <= ", last, ") for (;;) {");
            Declare(depth + 2, first, TypeTable::Int, name, counter);
            Statements(statement.body, depth + 2, owner, label);
            Line(depth + 2, "ls_continue_", label, ":;");
            Line(depth + 2, "if (", counter, " == ", last, ") break;");
            Line(depth + 2, "++", counter, ';');
        } else if (types.IsList(iterable.type)) {
            // Reads the count on every pass, like ipairs.
            const auto element = types.Element(iterable.type);
            const auto list = Temp("list", label), index = Temp("index", label);
            const auto item = [&] { Put("*(", Type(element), "*)(", list, "->data + ", index, " * (ls_int)", list, "->element_size)"); };
            Line(depth + 1, "LS_List* ", list, " = ", Emitted(iterable, owner), "; ls_int ", index, ';');
            Line(depth + 1, "for (", index, " = 0; ", index, " < ls_list_size(", list, "); ++", index, ") {");
            if (second) { Declare(depth + 2, first, TypeTable::Int, name, index); Declare(depth + 2, second, element, secondName, item); }
            else Declare(depth + 2, first, element, name, item);
            Statements(statement.body, depth + 2, owner);
        } else if (iterable.type == TypeTable::Table || iterable.type == TypeTable::Any) {
            // A table with one variable, or `any`, follows the Lua backend: pairs with two variables, ipairs with one.
            assert(second || iterable.type == TypeTable::Any);
            const auto source = Temp("iterable", label), cursor = Temp("cursor", label), key = Temp("key", label), item = Temp("item", label);
            Line(depth + 1, "LS_Value ", source, " = ", Boxed(iterable, owner), "; ls_wrap_bits ", cursor, " = 0; LS_Value ", key, ", ", item, ';');
            Line(depth + 1, "while (ls_value_next(", source, ", ", second ? 0 : 1, ", &", cursor, ", &", key, ", &", item, ")) {");
            if (second) { Declare(depth + 2, first, TypeTable::Any, name, key); Declare(depth + 2, second, TypeTable::Any, secondName, item); }
            else Declare(depth + 2, first, TypeTable::Any, name, item);
            Statements(statement.body, depth + 2, owner);
        } else throw std::runtime_error("Native C backend cannot iterate a value of type '" + Name(iterable.type) + "'");
        Line(depth + 1, '}');
        Line(depth, '}');
        CloseDebugScope(scope);
    }
    const FunctionRef& RefOf(const BoundFunction& function) const { return functionRefs.at(function.symbolIndex); }
    static std::vector<const BoundFunction*> Constructors(const BoundClass& type) {
        std::vector<const BoundFunction*> result;
        for (const auto& item : type.functions) if (item.isConstructor) result.push_back(&item);
        return result;
    }
    bool IsOverloaded(const BoundClass& type, const BoundFunction& function) const {
        return std::count_if(type.functions.begin(), type.functions.end(), [&](const auto& item) { return item.isConstructor == function.isConstructor && item.name == function.name; }) > 1;
    }
    // Structs in dependency order: a struct is defined after the structs it contains.
    bool IsEditableEnum(const BoundField& field) const { return field.isEditable && IsEnum(Find(field.type)); }
    // The members of a script or extern enum and their values.
    std::vector<std::pair<std::string_view, long long>> EnumMembers(std::string_view name) const {
        std::vector<std::pair<std::string_view, long long>> result;
        if (const auto bound = std::ranges::find(program.enums, name, &BoundEnum::name); bound != program.enums.end()) {
            for (const auto& member : bound->members) result.emplace_back(member.name, member.value);
            return result;
        }
        const auto external = std::ranges::find_if(externalTypes, [&](const ExternalType& type) { return type.isEnum && type.name == name; });
        assert(external != externalTypes.end());
        for (size_t i{}; i < external->members.size(); ++i) result.emplace_back(external->members[i].name, external->values.empty() ? static_cast<long long>(i) : external->values[i]);
        return result;
    }
    // ls_enuminfo_<E> (LS_EnumInfo) for each enum that an [Editable] field has: the host shows and saves it by member name.
    void EnumInfos() {
        std::vector<std::string_view> emitted;
        for (const auto* type : orderedClasses) for (const auto& field : type->fields) {
            if (!IsEditableEnum(field) || std::ranges::find(emitted, field.type) != emitted.end()) continue;
            emitted.push_back(field.type);
            const auto members = EnumMembers(field.type);
            const Id id{field.type};
            if (!members.empty()) {
                Line(0, "static const char* const ls_enumnames_", id, "[] = {", [&] { for (const auto& [member, value] : members) Put('"', member, "\", "); }, "};");
                Line(0, "static const ls_int ls_enumvalues_", id, "[] = {", [&] { for (const auto& [member, value] : members) Put("(ls_int)", value, ", "); }, "};");
            }
            Line(0, "static const LS_EnumInfo ls_enuminfo_", id, " = {\"", field.type, "\", ", members.size(), ", ",
                 [&] { if (members.empty()) Put("(const char* const*)0, (const ls_int*)0"); else Put("ls_enumnames_", id, ", ls_enumvalues_", id); }, "};");
        }
        if (!emitted.empty()) Line(0);
    }
    void StructDefinitions() {
        StringSet emitted;
        const auto define = [&](auto&& self, const BoundClass& value) -> void {
            if (!emitted.insert(value.name).second) return;
            for (const auto& field : value.fields) if (const auto inner = structs.find(field.type); !field.isStatic && inner != structs.end()) self(self, *inner->second);
            InstanceLayout(value);
        };
        for (const auto& value : program.classes) if (value.isStruct) define(define, value);
        // Before the extern class wrappers, which return ls_S_default() on a null receiver.
        for (const auto& value : program.classes) if (value.isStruct) {
            Line(0, "LS_", Id{value.name}, " ls_", Id{value.name}, "_default(void);");
            Line(0, "ls_bool ls_", Id{value.name}, "_eq(LS_", Id{value.name}, " a, LS_", Id{value.name}, " b);");
            if (value.hostDefault) Line(0, "extern LS_", Id{value.name}, " ls_native_", Id{value.name}, "_new(void);");
        }
    }
    // "struct LS_T { ... };" with the instance fields, inherited ones first; static fields live in module state.
    void InstanceLayout(const BoundClass& value) {
        const auto fields = InstanceFields(value);
        Line(0, "struct LS_", Id{value.name}, " {");
        if (!value.isStruct) Line(1, "const LS_TypeInfo* ls_type;");
        else if (fields.empty()) Line(1, "char ls_empty;");
        if (!value.isStruct && NativeBaseOf(value)) Line(1, "void* ls_native; /* LS_NativeObject.native */");
        for (const auto* field : fields) Line(1, CTypeName{field->type}, ' ', Id{field->name}, ';');
        Line(0, "};");
    }
    // "ls_T__init": runs the base constructor, the field initializers and the body on an allocated object.
    std::string InitName(const BoundClass& type, const BoundFunction* constructor) const {
        auto name = FunctionName(type.name, "_init");
        if (constructor && RefOf(*constructor).ordinal) name += "__" + std::to_string(RefOf(*constructor).ordinal);
        return name;
    }
    void InitSignature(const BoundClass& type, const BoundFunction* constructor) {
        Put("void ", InitName(type, constructor), "(LS_", Id{type.name}, "* self");
        if (constructor) for (const auto& parameter : constructor->parameters) Put(", ", [&] { ParameterDeclaration(parameter); });
        Put(')');
    }
    // The base constructor that `constructor` (null: the implicit one) runs: the one `: base(...)` chose, else the parameterless one.
    const BoundFunction* BaseConstructor(const BoundClass& base, const BoundFunction* constructor) const {
        if (constructor && constructor->baseConstructor) return functionRefs.at(*constructor->baseConstructor).function;
        const auto found = std::find_if(base.functions.begin(), base.functions.end(), [](const auto& item) { return item.isConstructor && item.parameters.empty(); });
        return found == base.functions.end() ? nullptr : &*found;
    }
    // An inout parameter is a pointer to the caller's copy.
    void ParameterDeclaration(const SyntaxParameter& parameter) { Put(CTypeName{parameter.type}, parameter.isInout ? "* " : " ", Id{parameter.name}); }
    void Parameters(const BoundFunction& function) {
        if (function.parameters.empty()) return Put("void");
        for (size_t i{}; i < function.parameters.size(); ++i) {
            if (i) Put(", ");
            ParameterDeclaration(function.parameters[i]);
        }
    }
    // Moves captured-and-assigned parameters into cells at the top of a body.
    void ParameterCells(const std::vector<SyntaxParameter>& parameters, size_t depth) {
        for (const auto& parameter : parameters) if (const auto* binding = ParameterBinding(parameter); IsCell(binding))
            Declare(depth, binding, binding->type, parameter.name, Id{parameter.name});
    }
    // Classes construct on the heap ("LS_T* ls_T_new(...)"); structs return by value ("LS_S ls_S_new(...)").
    void ConstructorSignature(const BoundClass& type, const BoundFunction* constructor) {
        const std::string name = constructor ? RefOf(*constructor).cName : FunctionName(type.name, "new");
        Put("LS_", Id{type.name}, type.isStruct ? " " : "* ", name, '(');
        if (constructor) Parameters(*constructor); else Put("void");
        Put(')');
    }
    // "R ls_T_name(LS_T* self, ...)"; static functions have no receiver.
    void MethodSignature(const BoundClass& type, const BoundFunction& function) {
        Put(CTypeName{function.returnType}, ' ', RefOf(function).cName, '(');
        if (function.isStatic) Parameters(function);
        else {
            Put("LS_", Id{type.name}, "* self");
            for (const auto& parameter : function.parameters) Put(", ", [&] { ParameterDeclaration(parameter); });
        }
        Put(')');
    }
    // Declared constructors, or one null entry for the implicit constructor of a class without any.
    static std::vector<const BoundFunction*> ClassConstructors(const BoundClass& type) {
        auto result = Constructors(type);
        if (result.empty() && !type.isStruct) result.push_back(nullptr);
        return result;
    }
    // "LS_T* ls_T__host_alloc(void)": a class that derives from an extern class has no ls_T_new.
    void HostAllocSignature(const BoundClass& type) { Put("LS_", Id{type.name}, "* ", FunctionName(type.name, "_host_alloc"), "(void)"); }
    void Prototypes(const BoundClass& value) {
        for (const auto* constructor : value.isStruct ? Constructors(value) : ClassConstructors(value)) {
            if (!value.isAbstract && !value.isStruct && NativeBaseOf(value)) Line(0, [&] { HostAllocSignature(value); }, ';');
            else if (!value.isAbstract) Line(0, [&] { ConstructorSignature(value, constructor); }, ';');
            if (!value.isStruct) Line(0, [&] { InitSignature(value, constructor); }, ';');
        }
        for (const auto& function : value.functions) if (!function.isConstructor && !function.isAbstract) Line(0, [&] { MethodSignature(value, function); }, ';');
    }
    // A method is reachable from `any` when its parameters and result can cross as LS_Value.
    bool IsDynamicCallable(const BoundFunction& function) const {
        const auto result = Find(function.returnType);
        if (result != TypeTable::Void && !TryKindName(result)) return false;
        return std::all_of(function.parameters.begin(), function.parameters.end(), [&](const auto& parameter) { return !parameter.isInout && TryKindName(Find(parameter.type)).has_value(); });
    }
    // A method gets an `any` thunk ("ls_invoke_<Owner>_<name>") when it has a body, is not overloaded in its class, and its types can cross as LS_Value.
    bool HasInvokeThunk(const BoundClass& type, const BoundFunction& function) const {
        return !type.isStruct && !function.isConstructor && !function.isStatic && !function.isAbstract && !IsOverloaded(type, function) && IsDynamicCallable(function);
    }
    // Methods `any` code can call on a type: the most-derived implementation of each name. Names overloaded anywhere in the chain are left out.
    std::vector<std::pair<const BoundClass*, const BoundFunction*>> DynamicMethods(const BoundClass& value) const {
        std::vector<std::pair<const BoundClass*, const BoundFunction*>> result;
        StringSet seen, overloaded;
        for (const auto* type = &value; type; type = BaseOf(*type)) for (const auto& function : type->functions) {
            if (function.isConstructor || function.isStatic) continue;
            const auto existing = std::find_if(result.begin(), result.end(), [&](const auto& item) { return item.second->name == function.name; });
            if (existing != result.end() && SlotKey(*existing->second) != SlotKey(function)) overloaded.insert(function.name);
            if (seen.insert(function.name).second) result.emplace_back(type, &function);
        }
        std::erase_if(result, [&](const auto& item) { return overloaded.contains(item.second->name) || !HasInvokeThunk(*item.first, *item.second); });
        return result;
    }
    // Field and method tables behind each object's LS_TypeInfo. Structs expose fields only; their offsets include the box header.
    void TypeInfo(const BoundClass& value) {
        const Id name{value.name};
        for (const auto& function : value.functions) {
            if (!HasInvokeThunk(value, function)) continue;
            const auto returnType = Find(function.returnType);
            Line(0, "static void ls_invoke_", name, '_', Id{function.name}, "(void* self, const LS_Value* arguments, LS_Value* result) {");
            const auto call = [&] {
                Put(RefOf(function).cName, "((LS_", name, "*)self");
                for (size_t i{}; i < function.parameters.size(); ++i) Put(", ", [&] { Unbox(Find(function.parameters[i].type), [&] { Put("arguments[", i, ']'); }); });
                Put(')');
            };
            if (function.parameters.empty()) Line(1, "(void)arguments;");
            if (returnType == TypeTable::Void) Line(1, call, "; (void)result;");
            else Line(1, "*result = ", [&] { BoxAs(returnType, call); }, ';');
            Line(0, '}');
        }
        const auto methods = value.isStruct ? decltype(DynamicMethods(value)){} : DynamicMethods(value);
        std::vector<const BoundField*> fields = InstanceFields(value);
        std::erase_if(fields, [&](const BoundField* field) { return !TryKindName(Find(field->type)).has_value(); });
        if (!fields.empty()) {
            Line(0, "static const LS_FieldInfo ls_fields_", name, "[] = {");
            for (const auto* field : fields)
                Line(1, "{\"", field->name, "\", ", value.isStruct ? "LS_STRUCT_PAYLOAD + " : "", "ls_offsetof(LS_", name, ", ", Id{field->name}, "), ", [&] { FieldDescriptor(Find(field->type)); }, ", ", field->isEditable ? "LS_FIELD_EDITABLE" : "0", ", ", [&] { CName(field->editor); }, ", ", [&] { CName(field->former); },
                     [&] { if (IsEditableEnum(*field)) Put(", &ls_enuminfo_", Id{field->type}); }, "},");
            Line(0, "};");
        }
        if (!methods.empty()) {
            Line(0, "static const LS_MethodInfo ls_methods_", name, "[] = {");
            for (const auto& [owner, function] : methods) Line(1, "{\"", function->name, "\", ls_invoke_", Id{owner->name}, '_', Id{function->name}, ", ", function->parameters.size(), "},");
            Line(0, "};");
        }
        const auto* slots = value.isStruct ? nullptr : &vtables.at(&value);
        if (slots && !slots->empty()) {
            Line(0, "static void* const ls_vtable_", name, "[] = {");
            for (const auto& slot : *slots) Line(1, [&] { if (slot.impl) Put("(void*)", RefOf(*slot.impl).cName); else Put("(void*)0"); }, ", /* ", slot.key, " */");
            Line(0, "};");
        }
        const auto* native = value.isStruct ? nullptr : NativeBaseOf(value);
        const auto overrides = native ? HostOverrides(value, *native) : std::vector<std::pair<std::string_view, const BoundFunction*>>{};
        if (!overrides.empty()) {
            Line(0, "static const LS_HostOverride ls_host_overrides_", name, "[] = {");
            for (const auto& [method, function] : overrides) Line(1, "{\"", method, "\", (void*)", RefOf(*function).cName, "},");
            Line(0, "};");
        }
        const auto* base = BaseOf(value);
        const std::string_view baseName = base ? std::string_view(base->name) : native ? std::string_view(value.baseName) : std::string_view();
        Line(0, "static const LS_TypeInfo ls_type_", name, " = {\"", value.name, "\", ", fields.size(), ", ", [&] { if (!fields.empty()) Put("ls_fields_", name); else Put("(const LS_FieldInfo*)0"); }, ", ",
            methods.size(), ", ", [&] { if (!methods.empty()) Put("ls_methods_", name); else Put("(const LS_MethodInfo*)0"); }, ", ", [&] { if (value.isStruct) Put("sizeof(LS_", name, ")"); else Put('0'); }, ", ",
            value.isStruct ? "LS_KIND_STRUCT" : "LS_KIND_OBJECT", ", ", [&] { if (!baseName.empty()) Put("&ls_type_", Id{baseName}); else Put("(const LS_TypeInfo*)0"); }, ", ",
            [&] { if (slots && !slots->empty()) Put("ls_vtable_", name); else Put("(void* const*)0"); }, [&] {
                if (!native) return;
                Put(", LS_TYPE_NATIVE_BASE, ", overrides.size(), ", ");
                if (!overrides.empty()) Put("ls_host_overrides_", name); else Put("(const LS_HostOverride*)0");
            }, "};");
        // The host reads the classes it creates (NativeModule::HostClassType).
        if (native || value.isExternal) Line(0, "const LS_TypeInfo* ", FunctionName(value.name, "_type"), "(void) { return &ls_type_", name, "; }");
    }
    // The most-derived script implementation of each extern virtual method that the class or its script bases override.
    std::vector<std::pair<std::string_view, const BoundFunction*>> HostOverrides(const BoundClass& value, const ExternalType& native) const {
        std::vector<std::pair<std::string_view, const BoundFunction*>> result;
        for (const auto& slot : vtables.at(&value)) {
            if (!slot.impl) continue;
            const auto* member = ExternalMember(native.name, slot.impl->name);
            if (member && member->kind == SymbolKind::Method && member->isVirtual && !member->isStatic) result.emplace_back(member->name, slot.impl);
        }
        return result;
    }
    // "Type name" or "Type? name" of an external parameter.
    static std::pair<std::string_view, std::string_view> SplitParameter(std::string_view parameter) {
        const auto label = SplitParameterLabel(parameter);
        return {label.type, label.name.empty() ? std::string_view("argument") : label.name};
    }
    // Host thunks take the host pointer for an extern class (null when null or destroyed), other types as they are.
    void HostType(std::string_view type) { if (natives.contains(type)) Put("void*"); else Put(CTypeName{type}); }
    void HostArgument(std::string_view type, std::string_view name) { if (natives.contains(type)) Put("ls_native_ptr(", Id{name}, ')'); else Put(Id{name}); }
    // Per extern member: a declaration of the host thunk "ls_native_<Owner>_<name>" and a wrapper "ls_<Owner>_<name>"
    // that checks the receiver. Fields use "__get_<field>" and "__set_<field>"; the constructor is "new".
    void NativeDeclarations() {
        for (const auto* type : orderedNatives) {
            StringSet emitted;
            for (const auto& member : type->members) {
                if (member.owner != type->name || !emitted.insert(member.name).second) continue; // inherited: declared with its own class
                const bool constructor = member.kind == SymbolKind::Method && member.name == type->name;
                if (member.kind == SymbolKind::Field) NativeField(*type, member);
                else if (constructor || member.kind == SymbolKind::Method) NativeMethod(*type, member, constructor);
            }
        }
        for (const auto& [native, member] : baseThunks) NativeMethod(*native, *member, false, BaseThunkName(member->name));
        if (!orderedNatives.empty()) Line(0);
    }
    void NativeField(const ExternalType& type, const Symbol& field) {
        const Id name{type.name}, member{field.name};
        const std::string label = type.name + "." + field.name;
        Line(0, "extern ", [&] { HostType(field.type); }, " ls_native_", name, "__get_", member, "(void* self);");
        Line(0, "extern void ls_native_", name, "__set_", member, "(void* self, ", [&] { HostType(field.type); }, " value);");
        Line(0, "static ", CTypeName{field.type}, ' ', Accessor(type.name, "get", field.name), "(LS_", name, "* self) { void* ls_host = ls_native_self(self, \"", label,
            "\"); if (!ls_host) return ", [&] { Default(Find(field.type)); }, "; return ls_native_", name, "__get_", member, "(ls_host); }");
        Line(0, "static ", CTypeName{field.type}, ' ', Accessor(type.name, "set", field.name), "(LS_", name, "* self, ", CTypeName{field.type}, " value) { void* ls_host = ls_native_self(self, \"", label,
            "\"); if (ls_host) ls_native_", name, "__set_", member, "(ls_host, ", [&] { HostArgument(field.type, "value"); }, "); return value; }");
    }
    // thunkName: the member part of the thunk and wrapper names; empty for the member's own name.
    void NativeMethod(const ExternalType& type, const Symbol& method, bool constructor, std::string_view thunkName = {}) {
        const Id name{type.name};
        const std::string_view memberName = !thunkName.empty() ? thunkName : constructor ? std::string_view("new") : std::string_view(method.name);
        const bool receiver = !constructor && !method.isStatic;
        const auto hostParameters = [&] {
            if (receiver) Put("void* self");
            for (size_t i{}; i < method.parameters.size(); ++i) { const auto [parameterType, parameterName] = SplitParameter(method.parameters[i]); if (i || receiver) Put(", "); HostType(parameterType); Put(' ', Id{parameterName}); }
            if (!receiver && method.parameters.empty()) Put("void");
        };
        const auto parameters = [&] {
            if (receiver) Put("LS_", name, "* self");
            for (size_t i{}; i < method.parameters.size(); ++i) { const auto [parameterType, parameterName] = SplitParameter(method.parameters[i]); if (i || receiver) Put(", "); Put(CTypeName{parameterType}, ' ', Id{parameterName}); }
            if (!receiver && method.parameters.empty()) Put("void");
        };
        const auto arguments = [&] {
            if (receiver) Put("ls_host");
            for (size_t i{}; i < method.parameters.size(); ++i) { const auto [parameterType, parameterName] = SplitParameter(method.parameters[i]); if (i || receiver) Put(", "); HostArgument(parameterType, parameterName); }
        };
        // The host returns the proxy of an extern class result.
        Line(0, "extern ", CTypeName{method.type}, " ls_native_", name, '_', Id{memberName}, '(', hostParameters, ");");
        const auto call = [&] { Put("ls_native_", name, '_', Id{memberName}, '(', arguments, ')'); };
        const bool returns = method.type != "void";
        if (!receiver) return Line(0, "static ", CTypeName{method.type}, ' ', Fn{type.name, memberName}, '(', parameters, ") { ", returns ? "return " : "", call, "; }");
        Line(0, "static ", CTypeName{method.type}, ' ', Fn{type.name, memberName}, '(', parameters, ") { void* ls_host = ls_native_self(self, \"", type.name, '.', method.name, "\"); if (!ls_host) return",
            [&] { if (returns) { Put(' '); Default(Find(method.type)); } }, "; ", returns ? "return " : "", call, "; }");
    }
    // One LS_TypeInfo per list type, so list headers and list fields can name their element type.
    void ListTypeInfos() {
        std::vector<TypeId> lists;
        for (TypeId id{}; id < types.Count(); ++id) if (types.IsList(id) && HasListTypeInfo(id)) lists.push_back(id);
        for (const auto id : lists) Line(0, "static const LS_TypeInfo ls_listtype_", id, ';');
        for (const auto id : lists) {
            Line(0, "static const LS_FieldInfo ls_listfields_", id, "[] = {{\"element\", 0, ", [&] { FieldDescriptor(types.Element(id)); }, "}};");
            Line(0, "static const LS_TypeInfo ls_listtype_", id, " = {\"", Name(id), "\", 1, ls_listfields_", id, ", 0, (const LS_MethodInfo*)0, 0, LS_KIND_LIST};");
        }
        if (!lists.empty()) Line(0);
    }

    // Source name of an export: overloads after the first carry their parameter types.
    static std::string ExportParameter(const SyntaxParameter& parameter) { return (parameter.isInout ? "inout " : "") + parameter.type; }
    std::string ExportName(const BoundClass& type, const BoundFunction& function) const {
        auto name = type.name + "." + (function.isConstructor ? "new" : function.name);
        if (RefOf(function).ordinal) {
            name += '(';
            for (size_t i{}; i < function.parameters.size(); ++i) { if (i) name += ','; name += ExportParameter(function.parameters[i]); }
            name += ')';
        }
        return name;
    }
    // Field initializers run through `self`, a pointer to the object or to the struct's local.
    void FieldInitializers(const BoundClass& value) {
        for (const auto& field : value.fields) if (!field.isStatic) Line(1, "self->", Id{field.name}, " = ", FieldInitializer(value, field), ';');
    }
    auto FieldInitializer(const BoundClass& owner, const BoundField& field) {
        return [this, &owner, &field] {
            const auto& initializer = program.Expression(field.initializer);
            const auto type = Find(field.type);
            if (initializer.kind == SyntaxExpression::Kind::Missing) Default(type); else Value(type, initializer, owner.name);
        };
    }
    void Definitions(const BoundClass& value, CCompilationResult& result) {
        const Id name{value.name};
        const auto constructors = Constructors(value);
        const auto exportConstructor = [&](const BoundFunction* constructor) {
            std::vector<std::string> parameters;
            if (constructor) for (const auto& parameter : constructor->parameters) parameters.push_back(ExportParameter(parameter));
            result.exports.push_back({constructor ? ExportName(value, *constructor) : value.name + ".new", constructor ? RefOf(*constructor).cName : FunctionName(value.name, "new"), value.name, std::move(parameters)});
        };
        // Structs build a value in place: field initializers, then the body.
        const auto emitStructConstructor = [&](const BoundFunction* constructor) {
            assert(constructor);
            DebugFunction(ExportName(value, *constructor), RefOf(*constructor).cName, [&] {
                Line(0, [&] { ConstructorSignature(value, constructor); }, " {");
                DebugParameters(constructor->parameters);
                Line(1, "LS_", name, " ls_value = ls_", name, "_default();");
                Line(1, "LS_", name, "* self = &ls_value; (void)self;");
                constructorResult = "ls_value"; functionReturn = TypeTable::Void;
                ParameterCells(constructor->parameters, 1);
                DebugSelf(1, CType(value.name) + "*", value.name, true);
                Statements(constructor->body, 1, value.name);
                Line(1, "return ls_value;"); Line(0, '}');
                constructorResult = {};
            });
            exportConstructor(constructor);
        };
        // Classes split construction: ls_T_new allocates and sets the type, so virtual calls dispatch to T while every
        // ls_*__init up the chain runs (base constructor, then own field initializers, then the body).
        const auto emitClassConstructor = [&](const BoundFunction* constructor) {
            const auto init = [&] {
                Line(0, [&] { InitSignature(value, constructor); }, " {");
                functionReturn = TypeTable::Void;
                if (constructor) { DebugParameters(constructor->parameters); ParameterCells(constructor->parameters, 1); }
                if (const auto* base = BaseOf(value)) {
                    const auto* target = BaseConstructor(*base, constructor);
                    const auto parameters = ParameterTypes(target);
                    Line(1, InitName(*base, target), "((LS_", Id{base->name}, "*)self", [&] {
                        if (!constructor) return;
                        for (NodeId i{}; i < constructor->baseArguments.count; ++i) Put(", ", Converted(i < parameters.size() ? parameters[i] : TypeTable::Unknown, program.expressions[constructor->baseArguments.first + i], value.name));
                    }, ");");
                }
                FieldInitializers(value);
                if (constructor) { DebugSelf(1, CType(value.name), value.name, false); Statements(constructor->body, 1, value.name); }
                Line(0, '}');
            };
            // The implicit constructor has no statements to debug.
            if (constructor) DebugFunction(ExportName(value, *constructor), InitName(value, constructor), init); else init();
            if (value.isAbstract) return;
            const auto allocate = [&] {
                Line(1, "LS_", name, "* self = (LS_", name, "*)ls_alloc(sizeof(LS_", name, "), ls_trace_", name, ");");
                Line(1, "if (!self) return (LS_", name, "*)0;");
                Line(1, "self->ls_type = &ls_type_", name, ';');
            };
            // The host links the object to its host object, then runs ls_T__init (see LuaSharpEngine::create).
            if (NativeBaseOf(value)) {
                Line(0, [&] { HostAllocSignature(value); }, " {"); allocate(); Line(1, "return self;"); Line(0, '}');
                return;
            }
            Line(0, [&] { ConstructorSignature(value, constructor); }, " {");
            allocate();
            Line(1, InitName(value, constructor), "(self", [&] { if (constructor) for (const auto& parameter : constructor->parameters) Put(", ", Id{parameter.name}); }, ");");
            Line(1, "return self;"); Line(0, '}');
            exportConstructor(constructor);
        };
        if (value.isStruct) {
            if (value.hostDefault) Line(0, "LS_", name, " ls_", name, "_default(void) { return ls_native_", name, "_new(); }");
            else {
                Line(0, "LS_", name, " ls_", name, "_default(void) {");
                Line(1, "LS_", name, " ls_value = {0}; LS_", name, "* self = &ls_value; (void)self;");
                functionReturn = TypeTable::Void;
                FieldInitializers(value);
                Line(1, "return ls_value;"); Line(0, '}');
            }
            Line(0, "ls_bool ls_", name, "_eq(LS_", name, " a, LS_", name, " b) {");
            // Same rule as ls_struct_equal: numbers by value, references by identity (strings are interned).
            Line(1, "return 1", [&] { for (const auto& field : value.fields) {
                if (field.isStatic) continue;
                if (IsStruct(Find(field.type))) Put(" && ls_", Id{field.type}, "_eq(a.", Id{field.name}, ", b.", Id{field.name}, ')');
                else if (field.type == "any") Put(" && ls_value_equal(a.", Id{field.name}, ", b.", Id{field.name}, ')');
                else Put(" && a.", Id{field.name}, " == b.", Id{field.name});
            } }, ';');
            Line(0, '}');
            for (const auto* constructor : constructors) emitStructConstructor(constructor);
        } else for (const auto* constructor : ClassConstructors(value)) emitClassConstructor(constructor);
        for (const auto& function : value.functions) {
            if (function.isConstructor || function.isAbstract) continue;
            DebugFunction(ExportName(value, function), RefOf(function).cName, [&] {
                Line(0, [&] { MethodSignature(value, function); }, " {");
                functionReturn = Find(function.returnType);
                DebugParameters(function.parameters);
                ParameterCells(function.parameters, 1);
                if (!function.isStatic) DebugSelf(1, CType(value.name) + (value.isStruct ? "*" : ""), value.name, value.isStruct);
                Statements(function.body, 1, value.name); Line(0, '}');
                functionReturn = TypeTable::Unknown;
            });
            std::vector<std::string> parameters;
            if (!function.isStatic) parameters.push_back(value.name);
            for (const auto& p : function.parameters) parameters.push_back(ExportParameter(p));
            result.exports.push_back({ExportName(value, function), RefOf(function).cName, function.returnType, std::move(parameters)});
        }
    }

    CCompilationResult Run() {
        Line(0, "/* generated by LuaSharp; C11 */");
        if (emitOptions.debugHooks) {
            // Before the runtime functions: the stack walk can follow rbp through every function of the module.
            Line(0, "static void ls_debug_text_begin(void) {}");
            Line(0, "#define LS_DEBUG_HOOKS");
        }
        Line(0, "#include <luasharp/runtime.h>");
        Line(0);
        for (const auto& value : program.classes) Line(0, "typedef struct LS_", Id{value.name}, " LS_", Id{value.name}, ';');
        for (const auto& value : externalTypes) if (!classes.contains(value.name) && !value.isEnum && !value.isNative) Line(0, "typedef struct LS_", Id{value.name}, " LS_", Id{value.name}, ';');
        // Extern classes share one C layout. Their type infos match the runtime's (NativeRuntime::NativeClass) by name.
        for (const auto* value : orderedNatives) {
            Line(0, "typedef LS_NativeObject LS_", Id{value->name}, ';');
            Line(0, "static const LS_TypeInfo ls_type_", Id{value->name}, " = {\"", value->name, "\", 0, (const LS_FieldInfo*)0, 0, (const LS_MethodInfo*)0, 0, LS_KIND_OBJECT, ",
                [&] { if (value->baseType.empty()) Put("(const LS_TypeInfo*)0"); else Put("&ls_type_", Id{value->baseType}); }, ", (void* const*)0, LS_TYPE_NATIVE};");
        }
        for (const auto& value : program.classes) Line(0, "static const LS_TypeInfo ls_type_", Id{value.name}, ';');
        if (!program.classes.empty() || !externalTypes.empty()) Line(0);
        ListTypeInfos();
        for (const auto& value : externalTypes) if (!value.isEnum && !value.isNative) for (const auto& member : value.members) if (member.kind == SymbolKind::Method) {
            const auto parameters = [&] {
                Put("LS_", Id{value.name}, "* self");
                for (const auto& parameter : member.parameters) {
                    const auto [type, name] = SplitParameter(parameter);
                    Put(", ", CTypeName{type}, SplitParameterLabel(parameter).isInout ? "* " : " ", Id{name});
                }
            };
            Line(0, "extern ", CTypeName{member.type}, ' ', Fn{value.name, member.name}, '(', parameters, ");");
        }
        if (!externalTypes.empty()) Line(0);
        for (const auto& value : program.enums) {
            for (const auto& member : value.members) Line(0, "#define LS_", Id{value.name}, '_', Id{member.name}, " ((ls_int)", member.value, ')');
        }
        for (const auto& value : externalTypes) if (value.isEnum) {
            assert(value.values.empty() || value.values.size() == value.members.size());
            for (size_t index = 0; index < value.members.size(); ++index)
                Line(0, "#define LS_", Id{value.name}, '_', Id{value.members[index].name}, " ((ls_int)", value.values.empty() ? static_cast<long long>(index) : value.values[index], ')');
        }
        if (!program.enums.empty()) Line(0);
        EnumInfos();
        StructDefinitions();
        NativeDeclarations();
        for (const auto* value : orderedClasses) InstanceLayout(*value);
        for (size_t i{}; i < lambdas.size(); ++i) if (!lambdaCaptures[i].empty()) {
            Line(0, "typedef struct LS_LambdaEnv_", i, " {");
            for (const auto* capture : lambdaCaptures[i]) Line(1, Type(capture->type), IsCell(capture) ? "* " : " ", Id{capture->name}, ';');
            Line(0, "} LS_LambdaEnv_", i, ';');
            Line(0, "static void ls_trace_lambda_env_", i, "(void* runtime_context, void* object) {");
            Line(1, "LS_LambdaEnv_", i, "* ls_env = (LS_LambdaEnv_", i, "*)object;");
            bool traced = false;
            for (const auto* capture : lambdaCaptures[i]) {
                if (IsCell(capture)) { traced = true; Line(1, "ls_runtime->mark(runtime_context, (void**)&ls_env->", Id{capture->name}, ");"); continue; }
                traced = MarkSlot(1, Name(capture->type), [&] { Put("ls_env->", Id{capture->name}); }) || traced;
            }
            if (!traced) Line(1, "(void)runtime_context; (void)ls_env;");
            Line(0, '}');
        }
        if (!program.classes.empty()) Line(0);
        for (const auto& value : program.classes) {
            if (value.isStruct) {
                if (HasReferences(value.name)) Line(0, "static void ls_trace_cell_", Id{value.name}, "(void* runtime_context, void* object) { ls_trace_struct_value(runtime_context, &ls_type_", Id{value.name}, ", (char*)object); }");
                continue;
            }
            Line(0, "static void ls_trace_", Id{value.name}, "(void* runtime_context, void* object) {");
            Line(1, "LS_", Id{value.name}, "* self = (LS_", Id{value.name}, "*)object;");
            bool tracesField = false;
            for (const auto* field : InstanceFields(value)) tracesField = MarkSlot(1, field->type, [&] { Put("self->", Id{field->name}); }) || tracesField;
            if (!tracesField) Line(1, "(void)runtime_context; (void)self;");
            Line(0, '}');
        }
        for (const auto name : dynamicNames) Line(0, "static ls_string ls_name_", Id{name}, ';');
        for (const auto name : typeOfTargets) Line(0, "static ", CTypeName{typeOfProvider->type}, " ls_typeof_", Id{name}, ';');
        const auto globals = [&] {
            std::vector<const BoundStatement*> result;
            for (const auto& statement : program.Statements(program.topLevel)) if (statement.kind == SyntaxStatement::Kind::Local) result.push_back(&statement);
            return result;
        }();
        Line(0, "typedef struct LS_ModuleState {");
        for (const auto* statement : globals) Line(1, Type(statement->type), ' ', Id{statement->Name()}, ';');
        for (const auto& [owner, field] : staticFields) Line(1, CTypeName{field->type}, " ls_static_", Id{owner->name}, "__", Id{field->name}, ';');
        if (globals.empty() && staticFields.empty()) Line(1, "unsigned char _unused;");
        Line(0, "} LS_ModuleState;");
        Line(0, "static LS_ModuleState ls_state;");
        Line(0, "void ls_module_trace_roots(void* runtime_context) {");
        bool tracesGlobal = !dynamicNames.empty() || !typeOfTargets.empty();
        for (const auto name : dynamicNames) Line(1, "ls_runtime->mark(runtime_context, (void**)&ls_name_", Id{name}, ");");
        for (const auto name : typeOfTargets) Line(1, "ls_runtime->mark(runtime_context, (void**)&ls_typeof_", Id{name}, ");");
        for (const auto* statement : globals) tracesGlobal = MarkSlot(1, Name(statement->type), [&] { Put("ls_state.", Id{statement->Name()}); }) || tracesGlobal;
        for (const auto& [owner, field] : staticFields) tracesGlobal = MarkSlot(1, field->type, StaticSlot(owner->name, field->name)) || tracesGlobal;
        if (!tracesGlobal) Line(1, "(void)runtime_context;");
        Line(0, '}');
        Line(0);
        for (size_t i{}; i < lambdas.size(); ++i) Line(0, "static void ls_lambda_", i, "(void* environment, const LS_Value* arguments, LS_Value* result);");
        if (!lambdas.empty()) Line(0);
        CCompilationResult result;
        for (const auto* value : orderedNatives) result.nativeClasses.push_back({value->name, value->baseType});
        for (const auto* value : orderedClasses) if (NativeBaseOf(*value)) result.hostClasses.push_back(value->name);
        for (const auto& value : program.classes) if (value.isExternal) result.externalStructs.push_back(value.name);
        for (const auto& value : program.classes) {
            NativeTypeLayout layout; layout.name = value.name; layout.baseName = value.baseName;
            for (const auto& field : value.fields) if (!field.isStatic) layout.fields.push_back({field.name, field.type});
            result.types.push_back(std::move(layout));
        }
        result.exports.push_back({"$abi_version", "ls_module_abi_version", "int", {}});
        result.exports.push_back({"$module_bind", "ls_module_bind", "int", {"$runtime_api"}});
        for (const auto* statement : globals) {
            const auto symbol = "ls_global_" + Identifier(statement->Name());
            Line(0, "void* ", symbol, "(void) { return &ls_state.", Id{statement->Name()}, "; }");
            result.globals.push_back({statement->Name(), symbol, Name(statement->type)});
        }
        // A static field is a global named "Type.field".
        for (const auto& [owner, field] : staticFields) {
            const auto symbol = "ls_global_static_" + Identifier(owner->name) + "__" + Identifier(field->name);
            Line(0, "void* ", symbol, "(void) { return &", StaticSlot(owner->name, field->name), "; }");
            result.globals.push_back({owner->name + "." + field->name, symbol, field->type});
        }
        for (const auto& value : program.classes) Prototypes(value);
        Line(0);
        // A class's method table can name its base's invoke thunks, so bases come first.
        for (const auto& value : program.classes) if (value.isStruct) TypeInfo(value);
        // The host copies these for the lists it makes (NativeModule::StructType).
        for (const auto& name : NativeStructTypes()) Line(0, "const LS_TypeInfo* ", FunctionName(name, "_type"), "(void) { return &ls_type_", name, "; }");
        for (const auto* value : orderedClasses) TypeInfo(*value);
        if (!program.classes.empty()) Line(0);
        for (const auto& value : program.classes) Definitions(value, result);
        for (size_t i{}; i < lambdas.size(); ++i) {
            const auto& lambda = *lambdas[i];
            const auto owner = lambdaOwners[i];
            DebugFunction(owner.empty() ? std::string("<lambda>") : std::string(owner) + ".<lambda>", "ls_lambda_" + std::to_string(i), [&] {
                Line(0, "static void ls_lambda_", i, "(void* environment, const LS_Value* arguments, LS_Value* result) {");
                if (lambdaCaptures[i].empty()) Line(1, "(void)environment;");
                else {
                    Line(1, "LS_LambdaEnv_", i, "* ls_env = (LS_LambdaEnv_", i, "*)environment;");
                    const auto self = std::find_if(lambdaCaptures[i].begin(), lambdaCaptures[i].end(), [](const auto* value) { return value->name == "this"; });
                    if (self != lambdaCaptures[i].end()) {
                        Line(1, Type((*self)->type), " self = ls_env->", Id{(*self)->name}, ';');
                        DebugSelf(1, CType(Name((*self)->type)), Name((*self)->type), false);
                    }
                }
                Line(1, "(void)arguments; (void)result;");
                const auto parameters = analysis.syntax.Parameters(*lambda.syntax);
                for (size_t parameter{}; parameter < parameters.size(); ++parameter)
                    Declare(1, ParameterBinding(parameters[parameter]), TypeTable::Any, parameters[parameter].name, [&] { Put("arguments[", parameter, ']'); });
                const auto returnType = types.Element(lambda.type);
                activeLambda = i;
                if (lambda.LambdaIsBlock()) Statements(lambda.lambdaBody, 1, owner);
                else {
                    DebugStatement(1, Child(lambda, 0).span.offset);
                    if (returnType == TypeTable::Void) Line(1, Emitted(Child(lambda, 0), owner), ';');
                    else Line(1, "*result = ", Boxed(Child(lambda, 0), owner), ';');
                }
                activeLambda.reset();
                Line(0, '}');
            });
        }
        if (!lambdas.empty()) Line(0);
        DebugFunction("<top-level>", "ls_module_init", [&] {
            Line(0, "int ls_module_init(void) {");
            // Static fields initialize before top-level code, in declaration order.
            for (const auto& [owner, field] : staticFields) Line(1, StaticSlot(owner->name, field->name), " = ", FieldInitializer(*owner, *field), ';');
            Statements(program.topLevel, 1, {}, {}, true); Line(1, "return 0;"); Line(0, '}');
        });
        result.exports.push_back({"$module_init", "ls_module_init", "int", {}});
        if (emitOptions.debugHooks) { DebugTables(); result.debug = std::move(debugInfo); }
        result.c = std::move(output); return result;
    }
};

std::runtime_error SyntaxError(const SyntaxDiagnostic& diagnostic) { return std::runtime_error(diagnostic.message + " at " + std::to_string(diagnostic.span.offset)); }
void ThrowFirstError(const Analysis& analysis) {
    if (!analysis.syntax.diagnostics.empty()) throw SyntaxError(analysis.syntax.diagnostics.front());
    const auto error = std::find_if(analysis.diagnostics.begin(), analysis.diagnostics.end(), [](const auto& value) { return value.severity == DiagnosticSeverity::Error; });
    if (error != analysis.diagnostics.end()) throw std::runtime_error(error->message + " at " + std::to_string(error->span.offset));
}

} // namespace

CCompilationResult CompileToC(std::string_view source) {
    return CompileToC(source, {});
}

CCompilationResult CompileToC(std::string_view source, const std::vector<ExternalType>& externalTypes) {
    return CompileToC(Analyze(source, externalTypes), externalTypes);
}

CCompilationResult CompileToC(const Analysis& analysis, const std::vector<ExternalType>& externalTypes, const CEmitOptions& options) {
    ThrowFirstError(analysis);
    for (const auto& type : externalTypes) {
        if (!type.isStruct || type.isNative) continue;
        const auto code = std::find_if(type.members.begin(), type.members.end(), [](const auto& member) { return member.kind != SymbolKind::Field || member.isStatic; });
        if (code != type.members.end()) throw std::runtime_error("A native module can use only external structs with instance fields: '" + type.name + "." + code->name + "' is not one");
    }
    // Extern classes declared in this source are described to the emitter like external ones.
    auto types = externalTypes;
    for (auto& type : ExportTypes(analysis)) if (type.isNative) types.push_back(std::move(type));
    return CEmitter(analysis, types, options).Run();
}

std::vector<ExternalType> DeclarationTypes(const std::vector<std::string>& sources, std::vector<ExternalType> externalTypes) {
    for (const auto& source : sources) {
        const auto analysis = Analyze(source, externalTypes);
        ThrowFirstError(analysis);
        if (!analysis.bound.Statements(analysis.bound.topLevel).empty()) throw std::runtime_error("A declaration source can declare only extern classes, structs and enums");
        for (const auto& type : analysis.bound.classes) {
            if (type.isExternal) continue;
            if (!type.isStruct) throw std::runtime_error("A declaration source can declare only extern classes, structs and enums: class '" + type.name + "' is a script class");
            if (!type.functions.empty()) throw std::runtime_error("A declared struct has only fields: '" + type.name + "' has a method or constructor");
            for (const auto& field : type.fields) {
                if (field.isStatic) throw std::runtime_error("A declared struct has only instance fields: '" + type.name + "." + field.name + "' is static");
                if (field.initializer) throw std::runtime_error("A field of a declared struct has no initializer: '" + type.name + "." + field.name + "'");
            }
        }
        for (auto& type : ExportTypes(analysis)) externalTypes.push_back(std::move(type));
    }
    return externalTypes;
}

} // namespace luasharp
