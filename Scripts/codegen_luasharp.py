"""LuaSharp engine binding: extern class declarations and typed C thunks for reflected classes.

See LuaSharp/docs/native-abi.md (Extern classes) for the thunk names and C types.
"""
import contextlib
import io
from dataclasses import dataclass, field

from codegen_lib import *

# Keywords from LuaSharp/src/Compiler.cpp, plus names that the language reserves for types.
LUASHARP_KEYWORDS = {
    "class", "struct", "static", "enum", "public", "private", "auto", "new", "if", "else", "while", "for", "in", "break",
    "continue", "return", "null", "true", "false", "virtual", "override", "abstract", "sealed", "is", "as", "extern",
}
LUASHARP_TYPE_NAMES = {"int", "float", "bool", "string", "table", "any", "void", "Vec2", "Vec3", "Vec2i", "Vec3i", "Quat", "Mat4x4", "math"}

# kind -> (LuaSharp spelling, C++ thunk type)
SCALARS = {
    INT_TYPE: ("int", "int64_t"),
    FLOAT_TYPE: ("float", "double"),
    BOOL_TYPE: ("bool", "uint8_t"),
    STRING_TYPE: ("string", "void*"),
    STRINGNAME_TYPE: ("string", "void*"),
    VEC2_TYPE: ("Vec2", "lsbind::Vec2"),
    VEC3_TYPE: ("Vec3", "lsbind::Vec3"),
    QUAT_TYPE: ("Quat", "lsbind::Quat"),
    MAT4_TYPE: ("Mat4x4", "lsbind::Mat4"),
}


@dataclass
class BoundType:
    cpp: CppType
    ls: str            # LuaSharp spelling
    thunk: str         # C++ type in the thunk signature
    enum: ClassDef | None = None
    object: ClassDef | None = None


@dataclass
class BoundMember:
    prop: Property
    name: str
    is_field: bool
    type: BoundType | None                     # field type, or the return type (None for void)
    params: list[tuple[BoundType, str]] = field(default_factory=list)
    readonly: bool = False


@dataclass
class BoundClass:
    cdef: ClassDef
    base: str
    members: list[BoundMember]
    skipped: list[str]


def enum_name(cdef: ClassDef) -> str:
    return cdef.classname.replace("::", "_")


def is_identifier(name: str) -> bool:
    return name.isidentifier() and name not in LUASHARP_KEYWORDS


def derives_from_classbase(cdef: ClassDef) -> bool:
    current: ClassDef | None = cdef
    while current is not None:
        if current.classname == "ClassBase":
            return True
        current = current.super_type_def
    return False


def is_bindable_class(cdef: ClassDef) -> bool:
    return cdef.object_type == ClassDef.TYPE_CLASS and derives_from_classbase(cdef) and cdef.classname not in LUASHARP_TYPE_NAMES


def bind_type(t: CppType) -> BoundType | None:
    """The LuaSharp type for a C++ value, or None when this milestone does not bind it."""
    if t.is_reference and not t.constant:
        return None  # out parameter
    if t.type == OTHER_CLASS_TYPE:
        if t.is_pointer and t.typename is not None and is_bindable_class(t.typename):
            return BoundType(t, t.typename.classname, "void*", object=t.typename)
        return None
    if t.is_pointer or t.template_args:
        return None
    if t.type == ENUM_TYPE:
        assert t.typename is not None
        return BoundType(t, enum_name(t.typename), "int64_t", enum=t.typename) if t.typename.properties else None
    if t.type in SCALARS:
        ls, thunk = SCALARS[t.type]
        return BoundType(t, ls, thunk)
    return None


def bind_class(cdef: ClassDef, cache: dict[str, BoundClass]) -> BoundClass:
    """Members that are declared by an ancestor, overloads and keyword names are skipped: LuaSharp extern classes have no overloads."""
    if cdef.classname in cache:
        return cache[cdef.classname]
    inherited: set[str] = set()
    ancestor = cdef.super_type_def
    while ancestor is not None:
        inherited.update(m.name for m in bind_class(ancestor, cache).members)
        ancestor = ancestor.super_type_def

    members: list[BoundMember] = []
    skipped: list[str] = []
    seen: set[str] = set()
    for p in cdef.properties:
        is_function = p.new_type.type == FUNCTION_TYPE
        label = f"{p.name}()" if is_function else p.name
        if not is_identifier(p.name):
            skipped.append(f"{label}: name is a LuaSharp keyword")
            continue
        if p.name in inherited:
            if not is_function:
                skipped.append(f"{label}: an ancestor declares it")
            continue  # overrides dispatch through the ancestor thunk
        if p.name in seen:
            skipped.append(f"{label}: overload")
            continue
        if is_function:
            returns_void = p.return_type.type == NONE_TYPE and not p.return_type.is_pointer
            ret = None if returns_void else bind_type(p.return_type)
            unbound = [a.get_raw_type_string() for a, _ in p.func_args if bind_type(a) is None]
            if ret is None and not returns_void:
                unbound.insert(0, p.return_type.get_raw_type_string())
            if unbound:
                skipped.append(f"{label}: {', '.join(unbound)}")
                continue
            params = []
            for i, (a, name) in enumerate(p.func_args):
                bound = bind_type(a)
                assert bound is not None
                params.append((bound, name if is_identifier(name) else f"arg{i}"))
            members.append(BoundMember(p, p.name, False, ret, params))
        else:
            if not p.script_readable:
                continue
            bound = bind_type(p.new_type)
            if bound is None or p.new_type.is_reference or (p.new_type.constant and not p.new_type.is_pointer):
                skipped.append(f"{label}: {p.new_type.get_raw_type_string()}")
                continue
            members.append(BoundMember(p, p.name, True, bound, readonly=not p.script_writable))
        seen.add(p.name)

    result = BoundClass(cdef, cdef.super_type_def.classname if cdef.super_type_def else "", members, skipped)
    cache[cdef.classname] = result
    return result


# ---- declarations ----

def declare_class(bound: BoundClass) -> str:
    c = bound.cdef
    out = f"// {c.classname} ({c.source_file})\n"
    out += f"extern class {c.classname}" + (f" : {bound.base}" if bound.base else "") + " {\n"
    for m in bound.members:
        if m.is_field:
            assert m.type is not None
            out += f"    {m.type.ls} {m.name};\n"
            continue
        modifiers = "static " if m.prop.is_static else "virtual " if m.prop.is_virtual else ""
        params = ", ".join(f"{t.ls} {n}" for t, n in m.params)
        out += f"    {modifiers}{m.type.ls if m.type else 'void'} {m.name}({params});\n"
    for s in bound.skipped:
        out += f"    // skipped {s}\n"
    return out + "}\n"


def declare_enum(cdef: ClassDef) -> str:
    return f"enum {enum_name(cdef)} {{ " + ", ".join(p.name for p in cdef.properties) + " }\n"


# ---- C++ thunks ----

def enum_table(e: ClassDef) -> str:
    return f"ls_enum_{enum_name(e)}"


def to_cpp(t: BoundType, value: str) -> str:
    """A thunk argument as the C++ parameter type."""
    k = t.cpp.type
    if t.object is not None:
        return f"lsbind::object<{t.object.classname}>({value})"
    if t.enum is not None:
        table = enum_table(t.enum)
        return f"static_cast<{t.enum.classname}>(lsbind::enum_value({table}, std::size({table}), {value}))"
    if k == INT_TYPE or k == FLOAT_TYPE:
        return f"static_cast<{t.cpp.type_string_raw}>({value})"
    if k == BOOL_TYPE:
        return f"({value} != 0)"
    if k == STRING_TYPE:
        return f"std::string(lsbind::view({value}))"
    if k == STRINGNAME_TYPE:
        return f"lsbind::name({value})"
    return f"lsbind::to_glm({value})"


def to_thunk(t: BoundType, value: str) -> str:
    """A C++ value as the thunk result type."""
    k = t.cpp.type
    if t.object is not None:
        return f"lsbind::proxy_of({value})"
    if t.enum is not None:
        table = enum_table(t.enum)
        return f"lsbind::enum_ordinal({table}, std::size({table}), static_cast<int64_t>({value}))"
    if k == INT_TYPE:
        return f"static_cast<int64_t>({value})"
    if k == FLOAT_TYPE:
        return f"static_cast<double>({value})"
    if k == BOOL_TYPE:
        return f"static_cast<uint8_t>({value} ? 1 : 0)"
    if k == STRING_TYPE:
        return f"lsbind::string({value})"
    if k == STRINGNAME_TYPE:
        return f"lsbind::string({value}.get_c_str())"
    return f"lsbind::to_ls({value})"


def thunk_symbol(owner: str, name: str) -> str:
    return f"ls_native_{owner}_{name}"


def write_thunks(bound: BoundClass) -> tuple[str, list[str]]:
    cls = bound.cdef.classname
    out = ""
    symbols: list[str] = []
    for m in bound.members:
        if m.is_field:
            assert m.type is not None
            raw = m.prop.new_type.get_raw_type_string()
            offset = f"ls_offset_{cls}_{m.name}"
            ref = f"lsbind::field<{raw}>(lsbind::object<{cls}>(self), {offset}())"
            get, set_ = thunk_symbol(cls, f"_get_{m.name}"), thunk_symbol(cls, f"_set_{m.name}")
            out += f"static uint16_t {offset}() {{ static const uint16_t offset = lsbind::field_offset({cls}::get_props(), \"{m.name}\"); return offset; }}\n"
            out += f"static {m.type.thunk} {get}(void* self) {{ return {to_thunk(m.type, ref)}; }}\n"
            if m.readonly:
                out += f"static void {set_}(void*, {m.type.thunk}) {{ lsbind::report_error(\"field '{cls}.{m.name}' is read-only\"); }}\n"
            else:
                out += f"static void {set_}(void* self, {m.type.thunk} value) {{ {ref} = {to_cpp(m.type, 'value')}; }}\n"
            symbols += [get, set_]
            continue
        params = ([] if m.prop.is_static else ["void* self"]) + [f"{t.thunk} {n}" for t, n in m.params]
        receiver = f"{cls}::" if m.prop.is_static else f"lsbind::object<{cls}>(self)->"
        call = f"{receiver}{m.name}(" + ", ".join(to_cpp(t, n) for t, n in m.params) + ")"
        symbol = thunk_symbol(cls, m.name)
        body = f"return {to_thunk(m.type, call)};" if m.type else f"{call};"
        out += f"static {m.type.thunk if m.type else 'void'} {symbol}({', '.join(params)}) {{ {body} }}\n"
        symbols.append(symbol)
    return out, symbols


# ---- script classes that derive from engine classes (ScriptImpl_<class> slots) ----

def slot_methods(cdef: ClassDef, cache: dict[str, BoundClass]) -> list[BoundMember]:
    """The virtual methods that script can override on a scriptable class: bound ones of the class and its ancestors.
    Methods that return a reference are left out: a slot returns a value."""
    result: list[BoundMember] = []
    current: ClassDef | None = cdef
    while current is not None:
        for m in bind_class(current, cache).members:
            if not m.is_field and m.prop.is_virtual and not m.prop.is_static and not m.prop.return_type.is_reference:
                result.append(m)
        current = current.super_type_def
    return result


def from_slot(t: BoundType, value: str) -> str:
    """A slot result (a LuaSharp C value) as the C++ type. Objects come back as proxies."""
    if t.object is not None:
        return f"lsbind::object<{t.object.classname}>(lsbind::native_of({value}))"
    return to_cpp(t, value)


def slot_call(m: BoundMember, index: int) -> str:
    """The statement in ScriptImpl_<class>::<method> that calls the script override in slot `index`."""
    signature = f"{m.type.thunk if m.type else 'void'} (*)(void*" + "".join(f", {t.thunk}" for t, _ in m.params) + ")"
    arguments = "".join(f", {to_thunk(t, cpp_name)}" for (t, _), (_, cpp_name) in zip(m.params, m.prop.func_args))
    call = f"ls_fn(get_script_proxy(){arguments})"
    body = f"return {from_slot(m.type, call)};" if m.type else f"{{ {call}; return; }}"
    return f"if (auto ls_fn = reinterpret_cast<{signature}>(ls_slots[{index}])) {body}"


def write_base_thunks(cls: str, slots: list[BoundMember]) -> tuple[str, list[str]]:
    """ls_native_<class>__base_<method>: `base.m()` in script, a call without virtual dispatch."""
    out = ""
    symbols: list[str] = []
    for m in slots:
        symbol = thunk_symbol(cls, f"_base_{m.name}")
        params = ["void* self"] + [f"{t.thunk} {n}" for t, n in m.params]
        call = f"lsbind::object<{cls}>(self)->{cls}::{m.name}(" + ", ".join(to_cpp(t, n) for t, n in m.params) + ")"
        body = f"return {to_thunk(m.type, call)};" if m.type else f"{call};"
        out += f"static {m.type.thunk if m.type else 'void'} {symbol}({', '.join(params)}) {{ {body} }}\n"
        symbols.append(symbol)
    return out, symbols


def used_enums(classes: list[BoundClass]) -> list[ClassDef]:
    found: dict[str, ClassDef] = {}
    for b in classes:
        for m in b.members:
            for t in ([m.type] if m.type else []) + [t for t, _ in m.params]:
                if t.enum is not None:
                    found[t.enum.classname] = t.enum
    return list(found.values())


def raw_string(text: str) -> str:
    assert ")ls\"" not in text
    return f"R\"ls({text})ls\""


def parse_all_classes(skip_dirs: list[str]) -> dict[str, ClassDef]:
    """Every reflected type with its members. A pass parses only its own headers, but member skips need the ancestors of other passes."""
    with contextlib.redirect_stdout(io.StringIO()):
        typenames = read_typenames_from_files(skip_dirs)
        ClassDef.fixup_types(typenames)
        for root, name in get_source_files_to_build(".", skip_dirs, True, 0.0):
            parse_file(f"{root}/{name}", typenames)
    return typenames


@dataclass
class Output:
    declarations: str                   # LuaSharp source, embedded in the C++
    cpp: str                            # appended to the pass's generated C++ file; empty when the pass binds nothing
    slots: dict[str, dict[str, str]]    # scriptable class -> method -> slot call for ScriptImpl_<class> (see slot_call)


def generate(pass_classes: list[ClassDef], all_types: dict[str, ClassDef], pass_name: str) -> Output:
    """The LuaSharp binding of the classes and enums of one codegen pass."""
    cache: dict[str, BoundClass] = {}
    classes = [bind_class(all_types[c.classname], cache) for c in pass_classes
               if c.classname in all_types and is_bindable_class(all_types[c.classname])]
    enums = [all_types[c.classname] for c in pass_classes if c.object_type == ClassDef.TYPE_ENUM
             and c.classname in all_types and all_types[c.classname].properties]

    chunks = [declare_enum(e) for e in enums] + [declare_class(b) for b in classes]
    declarations = f"// GENERATED by Scripts/codegen_luasharp.py from the {pass_name} codegen pass. Do not edit.\n" + "\n".join(chunks)

    if not chunks:
        return Output(declarations, "", {})
    cpp = "\n// ---- LuaSharp binding (Scripts/codegen_luasharp.py) ----\n"
    for e in used_enums(classes):
        values = ", ".join(f"static_cast<int64_t>({e.classname}::{p.name})" for p in e.properties)
        cpp += f"static const int64_t {enum_table(e)}[] = {{{values}}};\n"
    symbols: list[str] = []
    for b in classes:
        text, names = write_thunks(b)
        cpp += text
        symbols += names
    slots: dict[str, dict[str, str]] = {}
    scriptables: list[str] = []
    for b in classes:
        if not b.cdef.scriptable:
            continue
        cls = b.cdef.classname
        methods = slot_methods(b.cdef, cache)
        slots[cls] = {m.name: slot_call(m, i) for i, m in enumerate(methods)}
        text, names = write_base_thunks(cls, methods)
        cpp += text
        symbols += names
        cpp += static_array("const char*", f"ls_slot_names_{cls}", [f"\"{m.name}\"" for m in methods])
        scriptables.append(f"{{\"{cls}\", ls_slot_names_{cls}, &lsbind::attach<ScriptImpl_{cls}>}}")
    cpp += static_array("const char*", "ls_declarations", [raw_string(c) for c in chunks])
    cpp += static_array("lsbind::Symbol", "ls_symbols", [f"{{\"{s}\", reinterpret_cast<const void*>(&{s})}}" for s in symbols])
    cpp += static_array("lsbind::Class", "ls_classes", [f"{{\"{b.cdef.classname}\", \"{b.base}\"}}" for b in classes])
    cpp += static_array("lsbind::Scriptable", "ls_scriptables", scriptables)
    cpp += f"static const lsbind::Registrar ls_registrar{{\"{pass_name}\", ls_declarations, ls_symbols, ls_classes, ls_scriptables}};\n"
    return Output(declarations, cpp, slots)


def static_array(element: str, name: str, items: list[str]) -> str:
    """A span over the items; C++ has no empty arrays."""
    if not items:
        return f"static const std::span<{element} const> {name};\n"
    return f"static {element} const {name}[] = {{\n" + "".join(f"\t{i},\n" for i in items) + "};\n"
