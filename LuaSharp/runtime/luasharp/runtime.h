/* LuaSharp native runtime contract. Every generated module includes this header.
   Layouts and LS_RuntimeApi must match NativeRuntime.cpp; change LS_ABI_VERSION with them. */
#ifndef LUASHARP_RUNTIME_H
#define LUASHARP_RUNTIME_H

#define LS_ABI_VERSION 5

typedef signed long long ls_int;          /* public LuaSharp int: signed 64-bit */
typedef unsigned long long ls_wrap_bits;  /* internal arithmetic bits; never a LuaSharp value */
typedef double ls_float;
typedef unsigned char ls_bool;

typedef struct LS_String { ls_int length; char data[1]; } LS_String;
typedef LS_String* ls_string;
typedef void LS_Table;
typedef LS_Table* ls_table;

/* GC-managed dynamic array. Generated code reads data directly; only the runtime resizes it.
   element_kind: 0 plain scalar, 1 managed pointer, 2 LS_Value. */
typedef struct LS_List { ls_int count; ls_int capacity; ls_wrap_bits element_size; ls_wrap_bits element_kind; char* data; } LS_List;

/* kind: 0 null, 1 int, 2 float, 3 bool, 4 string, 5 object, 6 table, 7 closure, 8 list */
typedef struct LS_Value {
    ls_wrap_bits kind;
    union { ls_int integer; ls_float number; ls_wrap_bits boolean; void* pointer; } as;
} LS_Value;

typedef struct LS_Closure {
    void (*invoke)(void* environment, const LS_Value* arguments, LS_Value* result);
    void* environment;
} LS_Closure;

typedef void (*LS_TraceFunction)(void* runtime_context, void* object);

typedef struct LS_RuntimeApi {
    ls_wrap_bits abi_version;
    void* context;
    void* (*allocate)(void* context, ls_wrap_bits size, LS_TraceFunction trace);
    void (*mark)(void* context, void* object);
    void* (*intern)(void* context, const char* value);
    void* (*table_create)(void* context, ls_wrap_bits count, const LS_Value* pairs);
    void (*table_get)(void* context, void* table, const LS_Value* key, LS_Value* out);
    void (*table_set)(void* context, void* table, const LS_Value* key, const LS_Value* value);
    void (*print)(void* context, void* format, ls_wrap_bits count, const LS_Value* arguments);
    void (*report_error)(void* context, const char* message);
    int (*table_next)(void* context, void* table, ls_wrap_bits* cursor, LS_Value* key, LS_Value* value);
    void* (*list_create)(void* context, ls_wrap_bits element_size, ls_wrap_bits element_kind, ls_wrap_bits capacity);
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

static void* ls_alloc(ls_wrap_bits size, LS_TraceFunction trace) {
    void* value;
    if (!ls_runtime || !ls_runtime->allocate) return (void*)0;
    value = ls_runtime->allocate(ls_runtime->context, size, trace);
    if (!value) ls_report_error("LuaSharp allocation failed");
    return value;
}

static void ls_mark_value(void* context, const LS_Value* value) {
    if (value && value->kind >= 4 && value->kind <= 8) ls_runtime->mark(context, value->as.pointer);
}

static void ls_printf(ls_string format, ls_wrap_bits count, const LS_Value* arguments) {
    if (ls_runtime && ls_runtime->print) ls_runtime->print(ls_runtime->context, format, count, arguments);
}

/* ---- closures ---- */

static void ls_trace_closure(void* context, void* object) {
    LS_Closure* closure = (LS_Closure*)object;
    if (closure->environment) ls_runtime->mark(context, closure->environment);
}

static LS_Closure* ls_closure_create(void (*invoke)(void*, const LS_Value*, LS_Value*), void* environment) {
    LS_Closure* closure = (LS_Closure*)ls_alloc(sizeof(LS_Closure), ls_trace_closure);
    if (!closure) return (LS_Closure*)0;
    closure->invoke = invoke;
    closure->environment = environment;
    return closure;
}

static void ls_closure_call(LS_Closure* closure, const LS_Value* arguments, LS_Value* result) {
    if (!closure || !closure->invoke) { ls_report_error("attempt to call null closure"); return; }
    closure->invoke(closure->environment, arguments, result);
}

/* ---- tables ---- */

static ls_table ls_table_create(ls_wrap_bits count, const LS_Value* pairs) {
    return ls_runtime && ls_runtime->table_create ? (ls_table)ls_runtime->table_create(ls_runtime->context, count, pairs) : (ls_table)0;
}

static void ls_table_get(ls_table table, const LS_Value* key, LS_Value* out) {
    if (ls_runtime && ls_runtime->table_get) ls_runtime->table_get(ls_runtime->context, table, key, out);
}

static void ls_table_set(ls_table table, const LS_Value* key, const LS_Value* value) {
    if (ls_runtime && ls_runtime->table_set) ls_runtime->table_set(ls_runtime->context, table, key, value);
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

static ls_bool ls_value_equal(const LS_Value* a, const LS_Value* b) {
    if (a->kind == 1 && b->kind == 2) return (ls_float)a->as.integer == b->as.number;
    if (a->kind == 2 && b->kind == 1) return a->as.number == (ls_float)b->as.integer;
    if (a->kind != b->kind) return 0;
    if (a->kind == 0) return 1;
    if (a->kind == 1) return a->as.integer == b->as.integer;
    if (a->kind == 2) return a->as.number == b->as.number;
    if (a->kind == 3) return a->as.boolean == b->as.boolean;
    return a->as.pointer == b->as.pointer;
}

static ls_float ls_value_number(const LS_Value* value) {
    return value->kind == 1 ? (ls_float)value->as.integer : value->as.number;
}

static int ls_value_is_number(const LS_Value* value) { return value->kind == 1 || value->kind == 2; }

/* operation: 0 <, 1 <=, 2 >, 3 >= */
static ls_bool ls_value_compare(const LS_Value* a, int operation, const LS_Value* b) {
    ls_float left, right;
    if (!ls_value_is_number(a) || !ls_value_is_number(b)) { ls_report_error("dynamic comparison requires numbers"); return 0; }
    left = ls_value_number(a);
    right = ls_value_number(b);
    return operation == 0 ? left < right : operation == 1 ? left <= right : operation == 2 ? left > right : left >= right;
}

/* operation: 0 +, 1 -, 2 *, 3 /, 4 % */
static void ls_value_arithmetic(const LS_Value* a, int operation, const LS_Value* b, LS_Value* out) {
    if (!ls_value_is_number(a) || !ls_value_is_number(b)) { ls_report_error("dynamic arithmetic requires numbers"); *out = (LS_Value){0}; return; }
    if (operation == 4) {
        if (a->kind != 1 || b->kind != 1) { ls_report_error("dynamic remainder requires integers"); *out = (LS_Value){0}; return; }
        out->kind = 1;
        out->as.integer = ls_int_mod(a->as.integer, b->as.integer);
        return;
    }
    if (operation == 3 || a->kind == 2 || b->kind == 2) {
        ls_float left = ls_value_number(a), right = ls_value_number(b);
        out->kind = 2;
        out->as.number = operation == 0 ? left + right : operation == 1 ? left - right : operation == 2 ? left * right : left / right;
        return;
    }
    out->kind = 1;
    out->as.integer = operation == 0 ? ls_int_add(a->as.integer, b->as.integer) : operation == 1 ? ls_int_sub(a->as.integer, b->as.integer) : ls_int_mul(a->as.integer, b->as.integer);
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
static ls_string ls_to_string(const LS_Value* value) {
    return (ls_string)ls_runtime->to_string(ls_runtime->context, value);
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

static LS_List* ls_list_create(ls_wrap_bits element_size, ls_wrap_bits element_kind, ls_wrap_bits capacity) {
    return (LS_List*)ls_runtime->list_create(ls_runtime->context, element_size, element_kind, capacity);
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

#endif
