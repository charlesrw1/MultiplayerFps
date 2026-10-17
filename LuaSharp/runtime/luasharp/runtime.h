/* LuaSharp native runtime contract. Every generated module includes this header.
   Layouts and LS_RuntimeApi must match NativeRuntime.cpp; change LS_ABI_VERSION with them. */
#ifndef LUASHARP_RUNTIME_H
#define LUASHARP_RUNTIME_H

#define LS_ABI_VERSION 13

typedef signed long long ls_int;          /* public LuaSharp int: signed 64-bit */
typedef unsigned long long ls_wrap_bits;  /* internal arithmetic bits; never a LuaSharp value */
typedef double ls_float;
typedef unsigned char ls_bool;

typedef struct LS_String { ls_int length; char data[1]; } LS_String;
typedef LS_String* ls_string;
typedef void LS_Table;
typedef LS_Table* ls_table;

/* LS_Value.kind. Kinds from LS_KIND_STRING up hold a managed pointer. */
#define LS_KIND_NIL 0
#define LS_KIND_INT 1
#define LS_KIND_FLOAT 2
#define LS_KIND_BOOL 3
#define LS_KIND_STRING 4
#define LS_KIND_OBJECT 5
#define LS_KIND_TABLE 6
#define LS_KIND_CLOSURE 7
#define LS_KIND_LIST 8
#define LS_KIND_STRUCT 9
/* Field slot kinds beyond LS_KIND: packed native components. They never appear in an LS_Value. */
#define LS_SLOT_FLOAT32 16
#define LS_SLOT_INT32 17
/* A boxed struct is a GC object: LS_TypeInfo pointer, then the value at this offset. Its fields are traced through that LS_TypeInfo. */
#define LS_STRUCT_PAYLOAD 16
/* Element kind of an `any` slot: the slot holds a whole LS_Value. Other slots use the LS_KIND of their type. */
#define LS_ELEMENT_VALUE 0

typedef struct LS_Value {
    ls_wrap_bits kind;
    union { ls_int integer; ls_float number; ls_wrap_bits boolean; void* pointer; } as;
} LS_Value;

struct LS_TypeInfo;
/* GC-managed dynamic array. Generated code reads data directly; only the runtime resizes it.
   element_type describes object, struct and list elements, and is null for the other kinds. */
typedef struct LS_List { ls_int count; ls_int capacity; ls_wrap_bits element_size; ls_wrap_bits element_kind; const struct LS_TypeInfo* element_type; char* data; } LS_List;

/* Missing trailing arguments are nil; arity lets dynamic calls pad them. */
typedef struct LS_Closure {
    void (*invoke)(void* environment, const LS_Value* arguments, LS_Value* result);
    void* environment;
    ls_wrap_bits arity;
} LS_Closure;

/* Every class object starts with its LS_TypeInfo, so `any` code can check casts and reach members by name. */
typedef struct LS_FieldInfo {
    const char* name;
    ls_wrap_bits offset;
    ls_wrap_bits kind;                  /* LS_KIND of the field type */
    ls_wrap_bits element_kind;          /* list fields only */
    const struct LS_TypeInfo* type;     /* class and struct fields; the element type of list fields */
    ls_wrap_bits flags;                 /* LS_FIELD_* */
    const char* editor;                 /* [Editor("name")], or null */
    const char* former;                 /* [Former("old")], or null */
} LS_FieldInfo;
/* [Editable]: the host edits and saves the field in place. */
#define LS_FIELD_EDITABLE 1
typedef struct LS_MethodInfo {
    const char* name;
    void (*invoke)(void* self, const LS_Value* arguments, LS_Value* result); /* exactly arity arguments */
    ls_wrap_bits arity;
} LS_MethodInfo;
typedef struct LS_TypeInfo {
    const char* name;
    ls_wrap_bits field_count;
    const LS_FieldInfo* fields;
    ls_wrap_bits method_count;
    const LS_MethodInfo* methods;
    ls_wrap_bits size;                  /* structs: sizeof the value, without the box header */
    ls_wrap_bits kind;                  /* LS_KIND_OBJECT, LS_KIND_STRUCT, or LS_KIND_LIST for a list type (one "element" field) */
    const struct LS_TypeInfo* base;     /* classes: the base class, or null */
    void* const* vtable;                /* classes: one function per virtual slot; null for an abstract slot */
    ls_wrap_bits flags;                 /* LS_TYPE_* */
    ls_wrap_bits host_override_count;
    const struct LS_HostOverride* host_overrides; /* LS_TYPE_NATIVE_BASE: script overrides of extern virtual methods */
} LS_TypeInfo;
/* The objects of the type are LS_NativeObject proxies for host objects (extern classes). Type infos with this
   flag match by name; the host's NativeRuntime owns the one its proxies point to. */
#define LS_TYPE_NATIVE 1
/* A script class that derives from an extern class. Its objects start with LS_NativeObject, then the script fields. */
#define LS_TYPE_NATIVE_BASE 2
/* The most-derived script implementation of an extern virtual method: R function(LS_T* self, ...). */
typedef struct LS_HostOverride { const char* name; void* function; } LS_HostOverride;
/* A class object; a derived class starts with the fields of its base, in the same order. */
typedef struct LS_Object { const LS_TypeInfo* type; } LS_Object;
/* A script reference to a host object. The host makes one proxy for each object and sets native to null
   when it destroys the object. A destroyed proxy is null for ==, conditions, `is`, `as` and `any`. */
typedef struct LS_NativeObject { const LS_TypeInfo* type; void* native; } LS_NativeObject;

#define ls_offsetof(type, member) ((ls_wrap_bits)&((type*)0)->member)
/* Interned member name, cached in a module static `ls_name_<id>` that the module traces as a root. */
#define LS_NAME(id) (ls_name_##id ? ls_name_##id : (ls_name_##id = ls_intern(#id)))
#define LS_MAX_DYNAMIC_ARGUMENTS 16

typedef void (*LS_TraceFunction)(void* runtime_context, void* object);

typedef struct LS_RuntimeApi {
    ls_wrap_bits abi_version;
    void* context;
    void* (*allocate)(void* context, ls_wrap_bits size, LS_TraceFunction trace);
    /* Visits one reference slot. Trace functions pass the slot address, not its value, so the runtime can also update it. */
    void (*mark)(void* context, void** slot);
    void* (*intern)(void* context, const char* value);
    void* (*table_create)(void* context, ls_wrap_bits count, const LS_Value* pairs);
    void (*table_get)(void* context, void* table, const LS_Value* key, LS_Value* out);
    void (*table_set)(void* context, void* table, const LS_Value* key, const LS_Value* value);
    void (*print)(void* context, void* format, ls_wrap_bits count, const LS_Value* arguments);
    void (*report_error)(void* context, const char* message);
    int (*table_next)(void* context, void* table, ls_wrap_bits* cursor, LS_Value* key, LS_Value* value);
    void* (*list_create)(void* context, ls_wrap_bits element_size, ls_wrap_bits element_kind, const LS_TypeInfo* element_type, ls_wrap_bits capacity);
    void (*list_reserve)(void* context, void* list, ls_wrap_bits capacity);
    void* (*list_slice)(void* context, void* list, ls_int start, ls_int end);
    void* (*string_concat)(void* context, void* left, void* right);
    void* (*string_substring)(void* context, void* value, ls_int start, ls_int count, int has_count);
    void* (*string_case)(void* context, void* value, int upper);
    void* (*to_string)(void* context, const LS_Value* value);
    void (*to_number)(void* context, void* value, LS_Value* out);
} LS_RuntimeApi;

static LS_RuntimeApi* ls_runtime;

/* ---- module binding ---- */

ls_int ls_module_abi_version(void) { return LS_ABI_VERSION; }

int ls_module_bind(LS_RuntimeApi* runtime) {
    if (!runtime || runtime->abi_version != LS_ABI_VERSION) return -1;
    if (!runtime->allocate || !runtime->mark || !runtime->intern || !runtime->report_error || !runtime->print) return -1;
    if (!runtime->table_create || !runtime->table_get || !runtime->table_set || !runtime->table_next) return -1;
    if (!runtime->list_create || !runtime->list_reserve || !runtime->list_slice) return -1;
    if (!runtime->string_concat || !runtime->string_substring || !runtime->string_case || !runtime->to_string || !runtime->to_number) return -1;
    ls_runtime = runtime;
    return 0;
}

/* ---- errors and allocation ---- */

static void ls_report_error(const char* message) {
    if (ls_runtime && ls_runtime->report_error) ls_runtime->report_error(ls_runtime->context, message);
}

/* Reports the concatenation of up to four parts (null parts are skipped). */
static void ls_report_parts(const char* a, const char* b, const char* c, const char* d) {
    char message[192];
    const char* parts[4];
    int count = 0, part;
    parts[0] = a; parts[1] = b; parts[2] = c; parts[3] = d;
    for (part = 0; part < 4; ++part) {
        const char* p = parts[part];
        while (p && *p && count < (int)sizeof(message) - 1) message[count++] = *p++;
    }
    message[count] = 0;
    ls_report_error(message);
}

static const char* ls_kind_name(ls_wrap_bits kind) {
    static const char* names[] = {"nil", "int", "float", "bool", "string", "object", "table", "function", "list", "struct"};
    return kind <= LS_KIND_STRUCT ? names[kind] : "invalid value";
}

static void ls_kind_error(const char* expected, ls_wrap_bits actual) { ls_report_parts("expected ", expected, ", got ", ls_kind_name(actual)); }

static void* ls_alloc(ls_wrap_bits size, LS_TraceFunction trace) {
    void* value;
    if (!ls_runtime || !ls_runtime->allocate) return (void*)0;
    value = ls_runtime->allocate(ls_runtime->context, size, trace);
    if (!value) ls_report_error("LuaSharp allocation failed");
    return value;
}

static void ls_mark_value(void* context, LS_Value* value) {
    if (value && value->kind >= LS_KIND_STRING && value->kind <= LS_KIND_STRUCT) ls_runtime->mark(context, &value->as.pointer);
}

static void ls_printf(ls_string format, ls_wrap_bits count, const LS_Value* arguments) {
    if (ls_runtime && ls_runtime->print) ls_runtime->print(ls_runtime->context, format, count, arguments);
}

/* ---- boxing: typed value -> LS_Value ---- */

static LS_Value ls_nil(void) { LS_Value v; v.kind = LS_KIND_NIL; v.as.integer = 0; return v; }
static LS_Value ls_box_int(ls_int x) { LS_Value v; v.kind = LS_KIND_INT; v.as.integer = x; return v; }
static LS_Value ls_box_float(ls_float x) { LS_Value v; v.kind = LS_KIND_FLOAT; v.as.number = x; return v; }
static LS_Value ls_box_bool(ls_bool x) { LS_Value v; v.kind = LS_KIND_BOOL; v.as.boolean = x ? 1 : 0; return v; }

/* A null reference boxes as nil, as in Lua. */
static LS_Value ls_box_pointer(ls_wrap_bits kind, void* x) {
    LS_Value v;
    v.kind = x ? kind : LS_KIND_NIL;
    v.as.pointer = x;
    return v;
}

/* ---- unboxing: LS_Value -> typed value; a wrong kind reports an error and gives zero ---- */

static ls_int ls_unbox_int(LS_Value v) {
    if (v.kind == LS_KIND_INT) return v.as.integer;
    /* Lua math.tointeger: a float with an integer value converts. */
    if (v.kind == LS_KIND_FLOAT && v.as.number >= -9223372036854775808.0 && v.as.number < 9223372036854775808.0 && v.as.number == (ls_float)(ls_int)v.as.number) return (ls_int)v.as.number;
    ls_kind_error("int", v.kind);
    return 0;
}

static ls_float ls_unbox_float(LS_Value v) {
    if (v.kind == LS_KIND_FLOAT) return v.as.number;
    if (v.kind == LS_KIND_INT) return (ls_float)v.as.integer;
    ls_kind_error("float", v.kind);
    return 0.0;
}

static ls_bool ls_unbox_bool(LS_Value v) {
    if (v.kind == LS_KIND_BOOL) return v.as.boolean != 0;
    ls_kind_error("bool", v.kind);
    return 0;
}

/* Strings, tables and closures. nil gives null. */
static void* ls_unbox_pointer(LS_Value v, ls_wrap_bits kind) {
    if (v.kind == LS_KIND_NIL) return (void*)0;
    if (v.kind == kind) return v.as.pointer;
    ls_kind_error(ls_kind_name(kind), v.kind);
    return (void*)0;
}

static int ls_cstr_equal(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

/* One class. Each module emits its own copy of an extern class's type info, so those match by name. */
static int ls_class_same(const LS_TypeInfo* a, const LS_TypeInfo* b) {
    return a == b || (a && b && (a->flags & b->flags & LS_TYPE_NATIVE) && ls_cstr_equal(a->name, b->name));
}
/* True when type is target or derives from it. */
static int ls_is_a(const LS_TypeInfo* type, const LS_TypeInfo* target) {
    for (; type; type = type->base) if (ls_class_same(type, target)) return 1;
    return 0;
}

/* False for null and for a destroyed native object. */
static ls_bool ls_object_alive(const void* object) {
    return object && (!(((const LS_Object*)object)->type->flags & (LS_TYPE_NATIVE | LS_TYPE_NATIVE_BASE)) || ((const LS_NativeObject*)object)->native);
}
/* A destroyed native object reads as nil. */
static LS_Value ls_value_live(LS_Value v) {
    if (v.kind == LS_KIND_OBJECT && !ls_object_alive(v.as.pointer)) { v.kind = LS_KIND_NIL; v.as.pointer = (void*)0; }
    return v;
}
/* The host object behind a proxy, for a call to member; reports an error and gives null when it is destroyed. */
static void* ls_native_self(const void* object, const char* member) {
    if (ls_object_alive(object)) return ((const LS_NativeObject*)object)->native;
    ls_report_parts("access to '", member, "' on a destroyed or null object", 0);
    return (void*)0;
}
/* `==` on native objects: all destroyed objects are null. */
static ls_bool ls_native_same(const void* a, const void* b) { return a == b || (!ls_object_alive(a) && !ls_object_alive(b)); }
static void* ls_native_ptr(const void* object) { return ls_object_alive(object) ? ((const LS_NativeObject*)object)->native : (void*)0; }

static void* ls_unbox_object(LS_Value v, const LS_TypeInfo* type) {
    v = ls_value_live(v);
    if (v.kind == LS_KIND_NIL) return (void*)0;
    if (v.kind == LS_KIND_OBJECT && ls_is_a(((LS_Object*)v.as.pointer)->type, type)) return v.as.pointer;
    ls_report_parts("expected ", type->name, ", got ", v.kind == LS_KIND_OBJECT ? ((LS_Object*)v.as.pointer)->type->name : ls_kind_name(v.kind));
    return (void*)0;
}

/* `x is T` and `x as T` on a class value. */
static ls_bool ls_object_is(const void* object, const LS_TypeInfo* type) { return ls_object_alive(object) && ls_is_a(((const LS_Object*)object)->type, type); }
static void* ls_object_as(void* object, const LS_TypeInfo* type) { return ls_object_is(object, type) ? object : (void*)0; }

/* True for one LS_TypeInfo, or for structs and list types that match field by field (each module emits its own).
   Script classes match only by identity. */
static int ls_type_same(const LS_TypeInfo* a, const LS_TypeInfo* b, int depth) {
    ls_wrap_bits i;
    if (ls_class_same(a, b)) return 1;
    if (!a || !b || a->kind != b->kind || a->kind == LS_KIND_OBJECT || depth > 8) return 0;
    if (a->size != b->size || a->field_count != b->field_count || !ls_cstr_equal(a->name, b->name)) return 0;
    for (i = 0; i < a->field_count; ++i) {
        const LS_FieldInfo* x = &a->fields[i];
        const LS_FieldInfo* y = &b->fields[i];
        if (x->offset != y->offset || x->kind != y->kind || x->element_kind != y->element_kind || !ls_cstr_equal(x->name, y->name)) return 0;
        if (!ls_type_same(x->type, y->type, depth + 1)) return 0;
    }
    return 1;
}

static const char* ls_element_name(ls_wrap_bits kind, const LS_TypeInfo* type) {
    return type ? type->name : kind == LS_ELEMENT_VALUE ? "any" : ls_kind_name(kind);
}

/* Checks the element kind and, for object, struct and list elements, the element type. */
static LS_List* ls_unbox_list(LS_Value v, ls_wrap_bits element_kind, const LS_TypeInfo* element_type) {
    LS_List* list;
    if (v.kind == LS_KIND_NIL) return (LS_List*)0;
    if (v.kind != LS_KIND_LIST) { ls_report_parts("expected list, got ", ls_kind_name(v.kind), 0, 0); return (LS_List*)0; }
    list = (LS_List*)v.as.pointer;
    if (list->element_kind == element_kind && ls_type_same(list->element_type, element_type, 0)) return list;
    ls_report_parts("list element type mismatch: expected ", ls_element_name(element_kind, element_type), ", got ", ls_element_name(list->element_kind, list->element_type));
    return (LS_List*)0;
}

/* `v is T` on `any`. type is the class or struct, or the element type of a list (with element_kind). */
static ls_bool ls_value_is(LS_Value v, ls_wrap_bits kind, ls_wrap_bits element_kind, const LS_TypeInfo* type) {
    v = ls_value_live(v);
    if (v.kind != kind) return 0;
    if (kind == LS_KIND_OBJECT) return ls_is_a(((LS_Object*)v.as.pointer)->type, type);
    if (kind == LS_KIND_STRUCT) return ls_type_same(*(const LS_TypeInfo**)v.as.pointer, type, 0);
    if (kind == LS_KIND_LIST) return ((LS_List*)v.as.pointer)->element_kind == element_kind && ls_type_same(((LS_List*)v.as.pointer)->element_type, type, 0);
    return 1;
}

/* ---- structs: value types boxed on the GC heap only when they become `any` ---- */

/* Marks the references inside a struct value. Field offsets include the box header. */
static void ls_trace_struct_value(void* context, const LS_TypeInfo* type, char* value) {
    ls_wrap_bits i;
    for (i = 0; i < type->field_count; ++i) {
        const LS_FieldInfo* field = &type->fields[i];
        char* slot = value + field->offset - LS_STRUCT_PAYLOAD;
        if (field->kind == LS_ELEMENT_VALUE) ls_mark_value(context, (LS_Value*)slot);
        else if (field->kind == LS_KIND_STRUCT) ls_trace_struct_value(context, field->type, slot);
        else if (field->kind >= LS_KIND_STRING && field->kind <= LS_KIND_LIST) ls_runtime->mark(context, (void**)slot);
    }
}

static void ls_trace_struct_box(void* context, void* object) {
    ls_trace_struct_value(context, *(const LS_TypeInfo**)object, (char*)object + LS_STRUCT_PAYLOAD);
}

static ls_bool ls_value_equal(LS_Value a, LS_Value b);

/* Field-by-field equality of two values of one struct type: numbers by value, references by identity. */
static ls_bool ls_struct_equal(const LS_TypeInfo* type, const char* a, const char* b) {
    ls_wrap_bits i;
    for (i = 0; i < type->field_count; ++i) {
        const LS_FieldInfo* field = &type->fields[i];
        const char* x = a + field->offset - LS_STRUCT_PAYLOAD;
        const char* y = b + field->offset - LS_STRUCT_PAYLOAD;
        ls_bool same;
        switch (field->kind) {
        case LS_ELEMENT_VALUE: same = ls_value_equal(*(const LS_Value*)x, *(const LS_Value*)y); break;
        case LS_KIND_INT: same = *(const ls_int*)x == *(const ls_int*)y; break;
        case LS_KIND_FLOAT: same = *(const ls_float*)x == *(const ls_float*)y; break;
        case LS_KIND_BOOL: same = !*(const ls_bool*)x == !*(const ls_bool*)y; break;
        case LS_SLOT_FLOAT32: same = *(const float*)x == *(const float*)y; break;
        case LS_SLOT_INT32: same = *(const int*)x == *(const int*)y; break;
        case LS_KIND_STRUCT: same = ls_struct_equal(field->type, x, y); break;
        default: same = *(void* const*)x == *(void* const*)y; break;
        }
        if (!same) return 0;
    }
    return 1;
}

static void ls_copy_bytes(void* to, const void* from, ls_wrap_bits size) {
    char* out = (char*)to;
    const char* in = (const char*)from;
    ls_wrap_bits i;
    for (i = 0; i < size; ++i) out[i] = in[i];
}

static LS_Value ls_box_struct(const LS_TypeInfo* type, const void* value) {
    char* box = (char*)ls_alloc(LS_STRUCT_PAYLOAD + type->size, ls_trace_struct_box);
    if (!box) return ls_nil();
    *(const LS_TypeInfo**)box = type;
    ls_copy_bytes(box + LS_STRUCT_PAYLOAD, value, type->size);
    return ls_box_pointer(LS_KIND_STRUCT, box);
}

/* Copies the boxed value into out (type->size bytes). A wrong kind or type reports an error and zeroes out. */
static void ls_unbox_struct(LS_Value v, const LS_TypeInfo* type, void* out) {
    ls_wrap_bits i;
    if (v.kind == LS_KIND_STRUCT) {
        const LS_TypeInfo* actual = *(const LS_TypeInfo**)v.as.pointer;
        if (ls_type_same(actual, type, 0)) { ls_copy_bytes(out, (const char*)v.as.pointer + LS_STRUCT_PAYLOAD, type->size); return; }
        ls_report_parts("expected ", type->name, ", got ", actual->name);
    } else ls_report_parts("expected ", type->name, ", got ", ls_kind_name(v.kind));
    for (i = 0; i < type->size; ++i) ((char*)out)[i] = 0;
}

/* Loads a typed slot (field, list element or cell) as an LS_Value. type is for struct slots. */
static LS_Value ls_slot_load(const char* slot, ls_wrap_bits kind, const LS_TypeInfo* type) {
    switch (kind) {
    case LS_ELEMENT_VALUE: return *(const LS_Value*)slot;
    case LS_KIND_INT: return ls_box_int(*(const ls_int*)slot);
    case LS_KIND_FLOAT: return ls_box_float(*(const ls_float*)slot);
    case LS_KIND_BOOL: return ls_box_bool(*(const ls_bool*)slot);
    case LS_SLOT_FLOAT32: return ls_box_float(*(const float*)slot);
    case LS_SLOT_INT32: return ls_box_int(*(const int*)slot);
    case LS_KIND_STRUCT:
        if (type) return ls_box_struct(type, slot);
        ls_report_error("a list of structs cannot be read through `any`");
        return ls_nil();
    default: return ls_box_pointer(kind, *(void* const*)slot);
    }
}

/* Checked store into a typed slot. type is for object and struct slots, element_kind and type (the element type) for list slots. */
static void ls_slot_store(char* slot, ls_wrap_bits kind, ls_wrap_bits element_kind, const LS_TypeInfo* type, LS_Value v) {
    switch (kind) {
    case LS_ELEMENT_VALUE: *(LS_Value*)slot = v; return;
    case LS_KIND_INT: *(ls_int*)slot = ls_unbox_int(v); return;
    case LS_KIND_FLOAT: *(ls_float*)slot = ls_unbox_float(v); return;
    case LS_KIND_BOOL: *(ls_bool*)slot = ls_unbox_bool(v); return;
    case LS_SLOT_FLOAT32: *(float*)slot = (float)ls_unbox_float(v); return;
    case LS_SLOT_INT32: *(int*)slot = (int)ls_unbox_int(v); return;
    case LS_KIND_STRUCT: if (type) ls_unbox_struct(v, type, slot); return;
    case LS_KIND_OBJECT: *(void**)slot = ls_unbox_object(v, type); return;
    case LS_KIND_LIST: *(void**)slot = ls_unbox_list(v, element_kind, type); return;
    default: *(void**)slot = ls_unbox_pointer(v, kind); return;
    }
}

/* Lua truthiness: only nil and false are false. */
static ls_bool ls_value_truthy(LS_Value v) { v = ls_value_live(v); return !(v.kind == LS_KIND_NIL || (v.kind == LS_KIND_BOOL && !v.as.boolean)); }

/* ---- closures ---- */

static void ls_trace_closure(void* context, void* object) {
    LS_Closure* closure = (LS_Closure*)object;
    if (closure->environment) ls_runtime->mark(context, &closure->environment);
}

static LS_Closure* ls_closure_create(void (*invoke)(void*, const LS_Value*, LS_Value*), void* environment, ls_wrap_bits arity) {
    LS_Closure* closure = (LS_Closure*)ls_alloc(sizeof(LS_Closure), ls_trace_closure);
    if (!closure) return (LS_Closure*)0;
    closure->invoke = invoke;
    closure->environment = environment;
    closure->arity = arity;
    return closure;
}

/* arguments must hold at least closure->arity values. */
static LS_Value ls_closure_call(LS_Closure* closure, const LS_Value* arguments) {
    LS_Value result = ls_nil();
    if (!closure || !closure->invoke) { ls_report_error("attempt to call null closure"); return result; }
    closure->invoke(closure->environment, arguments, &result);
    return result;
}

static void ls_trace_cell_pointer(void* context, void* object) { ls_runtime->mark(context, (void**)object); }
static void ls_trace_cell_value(void* context, void* object) { ls_mark_value(context, (LS_Value*)object); }

/* Heap slot for a captured local that is also assigned, so closures and the enclosing scope share it. */
static void* ls_cell_create(ls_wrap_bits size, ls_wrap_bits kind) {
    return ls_alloc(size, kind == LS_ELEMENT_VALUE ? ls_trace_cell_value : (kind >= LS_KIND_STRING && kind <= LS_KIND_LIST) ? ls_trace_cell_pointer : (LS_TraceFunction)0);
}

/* ---- tables ---- */

static ls_table ls_table_create(void) {
    return (ls_table)ls_runtime->table_create(ls_runtime->context, 0, (const LS_Value*)0);
}

/* A missing key gives nil. */
static LS_Value ls_table_get(ls_table table, LS_Value key) {
    LS_Value out = ls_nil();
    ls_runtime->table_get(ls_runtime->context, table, &key, &out);
    return out;
}

/* Writing nil removes the entry. */
static void ls_table_set(ls_table table, LS_Value key, LS_Value value) {
    ls_runtime->table_set(ls_runtime->context, table, &key, &value);
}

/* Yields the next non-null entry after *cursor; returns 0 at the end. */
static int ls_table_next(ls_table table, ls_wrap_bits* cursor, LS_Value* key, LS_Value* value) {
    return ls_runtime->table_next(ls_runtime->context, table, cursor, key, value);
}

/* ---- integers: two's-complement wrapping without C signed-overflow UB ---- */

static ls_int ls_int_add(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a + (ls_wrap_bits)b); }
static ls_int ls_int_sub(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a - (ls_wrap_bits)b); }
static ls_int ls_int_mul(ls_int a, ls_int b) { return (ls_int)((ls_wrap_bits)a * (ls_wrap_bits)b); }
static ls_int ls_int_neg(ls_int a) { return (ls_int)(0ULL - (ls_wrap_bits)a); }
static ls_float ls_int_div(ls_int a, ls_int b) { return (ls_float)a / (ls_float)b; }

/* Lua floor modulo: the result has the sign of b. */
static ls_int ls_int_mod(ls_int a, ls_int b) {
    ls_int r;
    if (!b) { ls_report_error("integer remainder by zero"); return 0; }
    if (b == -1) return 0;
    r = a % b;
    if (r && ((r < 0) != (b < 0))) r = ls_int_add(r, b);
    return r;
}

/* ---- dynamic values ---- */

static ls_bool ls_value_equal(LS_Value a, LS_Value b) {
    a = ls_value_live(a); b = ls_value_live(b);
    if (a.kind == LS_KIND_INT && b.kind == LS_KIND_FLOAT) return (ls_float)a.as.integer == b.as.number;
    if (a.kind == LS_KIND_FLOAT && b.kind == LS_KIND_INT) return a.as.number == (ls_float)b.as.integer;
    if (a.kind != b.kind) return 0;
    if (a.kind == LS_KIND_NIL) return 1;
    if (a.kind == LS_KIND_INT) return a.as.integer == b.as.integer;
    if (a.kind == LS_KIND_FLOAT) return a.as.number == b.as.number;
    if (a.kind == LS_KIND_BOOL) return a.as.boolean == b.as.boolean;
    if (a.kind == LS_KIND_STRUCT) {
        const LS_TypeInfo* type = *(const LS_TypeInfo**)a.as.pointer;
        return ls_type_same(type, *(const LS_TypeInfo**)b.as.pointer, 0) && ls_struct_equal(type, (const char*)a.as.pointer + LS_STRUCT_PAYLOAD, (const char*)b.as.pointer + LS_STRUCT_PAYLOAD);
    }
    return a.as.pointer == b.as.pointer;
}

static ls_float ls_value_number(LS_Value value) {
    return value.kind == LS_KIND_INT ? (ls_float)value.as.integer : value.as.number;
}

static int ls_value_is_number(LS_Value value) { return value.kind == LS_KIND_INT || value.kind == LS_KIND_FLOAT; }

/* operation: 0 <, 1 <=, 2 >, 3 >= */
static ls_bool ls_value_compare(LS_Value a, int operation, LS_Value b) {
    ls_float left, right;
    if (!ls_value_is_number(a) || !ls_value_is_number(b)) { ls_report_error("dynamic comparison requires numbers"); return 0; }
    left = ls_value_number(a);
    right = ls_value_number(b);
    return operation == 0 ? left < right : operation == 1 ? left <= right : operation == 2 ? left > right : left >= right;
}

/* operation: 0 +, 1 -, 2 *, 3 /, 4 % */
static LS_Value ls_value_arithmetic(LS_Value a, int operation, LS_Value b) {
    if (!ls_value_is_number(a) || !ls_value_is_number(b)) { ls_report_error("dynamic arithmetic requires numbers"); return ls_nil(); }
    if (operation == 4) {
        if (a.kind != LS_KIND_INT || b.kind != LS_KIND_INT) { ls_report_error("dynamic remainder requires integers"); return ls_nil(); }
        return ls_box_int(ls_int_mod(a.as.integer, b.as.integer));
    }
    if (operation == 3 || a.kind == LS_KIND_FLOAT || b.kind == LS_KIND_FLOAT) {
        ls_float left = ls_value_number(a), right = ls_value_number(b);
        return ls_box_float(operation == 0 ? left + right : operation == 1 ? left - right : operation == 2 ? left * right : left / right);
    }
    return ls_box_int(operation == 0 ? ls_int_add(a.as.integer, b.as.integer) : operation == 1 ? ls_int_sub(a.as.integer, b.as.integer) : ls_int_mul(a.as.integer, b.as.integer));
}

static LS_Value ls_value_negate(LS_Value v) {
    if (v.kind == LS_KIND_INT) return ls_box_int(ls_int_neg(v.as.integer));
    if (v.kind == LS_KIND_FLOAT) return ls_box_float(-v.as.number);
    ls_report_error("dynamic negation requires a number");
    return ls_nil();
}

/* ---- strings: interned, so equality is pointer equality ---- */

static ls_string ls_intern(const char* value) {
    return ls_runtime && ls_runtime->intern ? (ls_string)ls_runtime->intern(ls_runtime->context, value) : (ls_string)0;
}

static ls_int ls_string_length(ls_string s) { return s ? s->length : 0; }
static ls_bool ls_string_equal(ls_string a, ls_string b) { return a == b; }
static ls_bool ls_string_truthy(ls_string s) { return s && s->length != 0; }

static ls_bool ls_string_starts_with(ls_string s, ls_string prefix) {
    ls_int i;
    if (!s || !prefix || prefix->length > s->length) return 0;
    for (i = 0; i < prefix->length; ++i) if (s->data[i] != prefix->data[i]) return 0;
    return 1;
}

static ls_bool ls_string_ends_with(ls_string s, ls_string suffix) {
    ls_int i, offset;
    if (!s || !suffix || suffix->length > s->length) return 0;
    offset = s->length - suffix->length;
    for (i = 0; i < suffix->length; ++i) if (s->data[offset + i] != suffix->data[i]) return 0;
    return 1;
}

static ls_bool ls_string_contains(ls_string s, ls_string part) {
    ls_int i, j;
    if (!s || !part) return 0;
    if (!part->length) return 1;
    for (i = 0; i + part->length <= s->length; ++i) {
        for (j = 0; j < part->length && s->data[i + j] == part->data[j]; ++j) {}
        if (j == part->length) return 1;
    }
    return 0;
}

static ls_string ls_string_concat(ls_string a, ls_string b) {
    return (ls_string)ls_runtime->string_concat(ls_runtime->context, a, b);
}

/* Lua string.sub(s, start + 1, has_count ? start + count : -1) */
static ls_string ls_string_substring(ls_string s, ls_int start, ls_int count, int has_count) {
    return (ls_string)ls_runtime->string_substring(ls_runtime->context, s, start, count, has_count);
}

static ls_string ls_string_case(ls_string s, int upper) {
    return (ls_string)ls_runtime->string_case(ls_runtime->context, s, upper);
}

/* Lua 5.4 tostring() */
static ls_string ls_to_string(LS_Value value) {
    return (ls_string)ls_runtime->to_string(ls_runtime->context, &value);
}

static ls_int ls_string_to_int(ls_string s) {
    LS_Value v;
    ls_runtime->to_number(ls_runtime->context, s, &v);
    if (v.kind == 1) return v.as.integer;
    if (v.kind == 2) return (ls_int)v.as.number;
    ls_report_error("string is not a number");
    return 0;
}

static ls_float ls_string_to_float(ls_string s) {
    LS_Value v;
    ls_runtime->to_number(ls_runtime->context, s, &v);
    if (v.kind == 1) return (ls_float)v.as.integer;
    if (v.kind == 2) return v.as.number;
    ls_report_error("string is not a number");
    return 0.0;
}

/* ---- lists ---- */

static LS_Value ls_list_scratch; /* target of failed list accesses, so callers never dereference null */

static char* ls_list_fault(const char* message) {
    ls_report_error(message);
    ls_list_scratch = (LS_Value){0};
    return (char*)&ls_list_scratch;
}

static LS_List* ls_list_create(ls_wrap_bits element_size, ls_wrap_bits element_kind, const LS_TypeInfo* element_type, ls_wrap_bits capacity) {
    return (LS_List*)ls_runtime->list_create(ls_runtime->context, element_size, element_kind, element_type, capacity);
}

static ls_int ls_list_size(LS_List* list) {
    if (!list) { ls_report_error("attempt to use a null list"); return 0; }
    return list->count;
}

/* Element address; a negative index counts from the end. */
static char* ls_list_at(LS_List* list, ls_int index) {
    if (!list) return ls_list_fault("attempt to index a null list");
    if (index < 0) index += list->count;
    if (index < 0 || index >= list->count) return ls_list_fault("list index out of range");
    return list->data + index * (ls_int)list->element_size;
}

/* Address of a new last element; grows capacity by doubling. */
static char* ls_list_push(LS_List* list) {
    if (!list) return ls_list_fault("attempt to push to a null list");
    if (list->count == list->capacity) ls_runtime->list_reserve(ls_runtime->context, list, list->capacity ? (ls_wrap_bits)list->capacity * 2 : 4);
    return list->data + (list->count++) * (ls_int)list->element_size;
}

/* Address of the removed last element, valid until the next push. */
static char* ls_list_pop(LS_List* list) {
    if (!list || !list->count) return ls_list_fault("pop_back on an empty list");
    --list->count;
    return list->data + list->count * (ls_int)list->element_size;
}

static void ls_list_clear(LS_List* list) {
    if (!list) { ls_report_error("attempt to clear a null list"); return; }
    list->count = 0;
}

/* Elements [start, end); negative bounds count from the end. */
static LS_List* ls_list_slice(LS_List* list, ls_int start, ls_int end) {
    return (LS_List*)ls_runtime->list_slice(ls_runtime->context, list, start, end);
}

/* ---- dynamic access on `any`: tables, and class objects through their LS_TypeInfo ---- */

static int ls_name_is(ls_string name, const char* text) {
    ls_int i;
    for (i = 0; i < name->length; ++i) if (text[i] != name->data[i]) return 0;
    return text[name->length] == 0;
}

static const LS_FieldInfo* ls_find_field(const LS_TypeInfo* type, ls_string name) {
    ls_wrap_bits i;
    for (i = 0; i < type->field_count; ++i) if (ls_name_is(name, type->fields[i].name)) return &type->fields[i];
    return (const LS_FieldInfo*)0;
}

static const LS_MethodInfo* ls_find_method(const LS_TypeInfo* type, ls_string name) {
    ls_wrap_bits i;
    for (i = 0; i < type->method_count; ++i) if (ls_name_is(name, type->methods[i].name)) return &type->methods[i];
    return (const LS_MethodInfo*)0;
}

/* target[key], or target.field when target is an object and key a string. */
static LS_Value ls_value_get(LS_Value target, LS_Value key) {
    if (target.kind == LS_KIND_TABLE) return ls_table_get((ls_table)target.as.pointer, key);
    if ((target.kind == LS_KIND_OBJECT || target.kind == LS_KIND_STRUCT) && key.kind == LS_KIND_STRING) {
        const LS_TypeInfo* type = ((LS_Object*)target.as.pointer)->type;
        const LS_FieldInfo* field = ls_find_field(type, (ls_string)key.as.pointer);
        if (field) return ls_slot_load((const char*)target.as.pointer + field->offset, field->kind, field->type);
        ls_report_parts("no field '", ((ls_string)key.as.pointer)->data, "' on ", type->name);
        return ls_nil();
    }
    ls_report_parts("attempt to index a ", ls_kind_name(target.kind), " value", 0);
    return ls_nil();
}

static void ls_value_set(LS_Value target, LS_Value key, LS_Value value) {
    if (target.kind == LS_KIND_TABLE) { ls_table_set((ls_table)target.as.pointer, key, value); return; }
    /* A box is a copy of a struct value; changing it would change every `any` that shares the box, and table keys. */
    if (target.kind == LS_KIND_STRUCT) { ls_report_parts("cannot assign a field of a boxed ", (*(const LS_TypeInfo**)target.as.pointer)->name, "; unbox it first", 0); return; }
    if (target.kind == LS_KIND_OBJECT && key.kind == LS_KIND_STRING) {
        const LS_TypeInfo* type = ((LS_Object*)target.as.pointer)->type;
        const LS_FieldInfo* field = ls_find_field(type, (ls_string)key.as.pointer);
        if (field) { ls_slot_store((char*)target.as.pointer + field->offset, field->kind, field->element_kind, field->type, value); return; }
        ls_report_parts("no field '", ((ls_string)key.as.pointer)->data, "' on ", type->name);
        return;
    }
    ls_report_parts("attempt to index a ", ls_kind_name(target.kind), " value", 0);
}

/* Calls a closure value. Missing arguments are nil. */
static LS_Value ls_value_call(LS_Value callee, const LS_Value* arguments, ls_wrap_bits count) {
    LS_Value padded[LS_MAX_DYNAMIC_ARGUMENTS];
    LS_Closure* closure;
    ls_wrap_bits i;
    if (callee.kind != LS_KIND_CLOSURE) { ls_report_parts("attempt to call a ", ls_kind_name(callee.kind), " value", 0); return ls_nil(); }
    closure = (LS_Closure*)callee.as.pointer;
    if (count < closure->arity) {
        if (closure->arity > LS_MAX_DYNAMIC_ARGUMENTS) { ls_report_error("too many arguments for a dynamic call"); return ls_nil(); }
        for (i = 0; i < closure->arity; ++i) padded[i] = i < count ? arguments[i] : ls_nil();
        arguments = padded;
    }
    return ls_closure_call(closure, arguments);
}

/* Lua receiver:name(...). An object calls its method; a table calls receiver[name] with the table first. */
static LS_Value ls_value_call_member(LS_Value receiver, ls_string name, const LS_Value* arguments, ls_wrap_bits count) {
    LS_Value padded[LS_MAX_DYNAMIC_ARGUMENTS + 1];
    LS_Value result = ls_nil();
    ls_wrap_bits i;
    if (count > LS_MAX_DYNAMIC_ARGUMENTS) { ls_report_error("too many arguments for a dynamic call"); return result; }
    if (receiver.kind == LS_KIND_OBJECT) {
        const LS_TypeInfo* type = ((LS_Object*)receiver.as.pointer)->type;
        const LS_MethodInfo* method = ls_find_method(type, name);
        if (!method) { ls_report_parts("no method '", name->data, "' on ", type->name); return result; }
        for (i = 0; i < method->arity; ++i) padded[i] = i < count ? arguments[i] : ls_nil();
        method->invoke(receiver.as.pointer, padded, &result);
        return result;
    }
    if (receiver.kind == LS_KIND_TABLE) {
        padded[0] = receiver;
        for (i = 0; i < count; ++i) padded[i + 1] = arguments[i];
        return ls_value_call(ls_table_get((ls_table)receiver.as.pointer, ls_box_pointer(LS_KIND_STRING, name)), padded, count + 1);
    }
    ls_report_parts("attempt to call method '", name->data, "' on a ", ls_kind_name(receiver.kind));
    return result;
}

/* foreach over `any`. ordered follows Lua ipairs (keys 1.. up to the first nil); otherwise Lua pairs.
   A list yields 1-based keys, the same as the Lua backend. */
static int ls_value_next(LS_Value iterable, int ordered, ls_wrap_bits* cursor, LS_Value* key, LS_Value* value) {
    if (iterable.kind == LS_KIND_LIST) {
        LS_List* list = (LS_List*)iterable.as.pointer;
        if ((ls_int)*cursor >= list->count) return 0;
        *value = ls_slot_load(list->data + *cursor * list->element_size, list->element_kind, list->element_type);
        *key = ls_box_int((ls_int)++*cursor);
        return 1;
    }
    if (iterable.kind == LS_KIND_TABLE) {
        if (!ordered) return ls_table_next((ls_table)iterable.as.pointer, cursor, key, value);
        *key = ls_box_int((ls_int)*cursor + 1);
        *value = ls_table_get((ls_table)iterable.as.pointer, *key);
        if (value->kind == LS_KIND_NIL) return 0;
        ++*cursor;
        return 1;
    }
    ls_report_parts("attempt to iterate a ", ls_kind_name(iterable.kind), " value", 0);
    return 0;
}

#include "vecmath.h"

#endif
