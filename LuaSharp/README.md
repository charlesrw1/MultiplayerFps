# LuaSharp

A standalone C++20 compiler for a typed, brace-based language. It transpiles to C and compiles that C in memory with libtcc. It does not include, link to, or otherwise depend on CsRemake engine source.

Build the `LuaSharp` project in Visual Studio, or from the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' LuaSharp\LuaSharp.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
.\x64\Debug\LuaSharp.exe .\LuaSharp\native-example.ls
.\x64\Debug\LuaSharp.exe .\LuaSharp\native-example.ls -o .\x64\Debug\native-example.generated.c
.\x64\Debug\LuaSharp.exe emit-c .\LuaSharp\example.ls
.\x64\Debug\LuaSharpTests.exe
```

Passing a `.ls` file directly checks it, prints all errors and warnings with line/column locations, writes generated C beside the script (or to `-o`), compiles it in memory with libtcc, and runs its module initializer. `run` and `run-native` are explicit aliases for that workflow. `emit-c` only writes the generated C.

The compiler supports the syntax in `example.ls`: classes and inheritance, enums with inferred or explicit integer values, expression-bodied and multiline brace-bodied lambdas, enforced public/private sections, typed fields, parameters and locals, constructors, methods, `auto`, `if`/`else if`/`else`, `while`, C-style `for`, value or zero-based index/value iteration over lists, key/value iteration over tables, inclusive integer ranges such as `1..10`, `break`, `continue`, `return`, `new`, `null`, arrays, indexing/slices, populated list literals, and Lua-style positional/keyed table values.

## Native backend

`emit-c` checks the source normally and writes C11 generated directly from the bound semantic tree. `run-native` loads `libtcc.dll`, compiles that C in memory, relocates it, and runs the generated module initializer without creating object files or launching another process. Install the pinned official Win64 TinyCC package with the checksum-verifying setup script:

```powershell
& .\LuaSharp\scripts\install-libtcc.ps1
.\x64\Debug\LuaSharp.exe run-native .\LuaSharp\native-example.ls
```

The build copies `tools\tcc` (`libtcc.dll`, `lib\`, `include\`) to `tcc\` beside the executable (`TinyCC.targets`). After an explicit API path, the loader checks `LUASHARP_LIBTCC` (a full DLL path), then `<exe dir>\tcc\libtcc.dll`. It does not use the working directory. libtcc finds its runtime relative to its own DLL, so a lone `libtcc.dll` without `lib\libtcc1-64.a` is rejected.

Host code can use `NativeModule::FindSymbol` with the export metadata returned by `CompileToC`. Generated names are stable (`Class.method` becomes `ls_Class_method`, and constructors become `ls_Class_new`), which provides the function-slot boundary needed for hot reload. A module owns its TinyCC state, so its function pointers remain valid until that module is destroyed.

The versioned runtime contract, scalar semantics, module state, external-symbol rules, and GC/reload direction are specified in [`docs/native-abi.md`](docs/native-abi.md).

LuaSharp `int` is a signed 64-bit native value. Generated arithmetic uses private unsigned bit operations only to guarantee two's-complement wrapping without invoking C signed-overflow undefined behavior; unsigned integers are not exposed as a LuaSharp type. `ReloadableNativeProgram` provides stable atomic function slots, validates signatures/class layouts/global types, and retains old code generations so already-loaded calls can finish safely.

The native slice supports these features:
- primitives, enums, and classes with constructors, methods and single inheritance (see below)
- top-level initialization
- `if`, `while`, C-style `for`, `break` and `continue`
- foreach loops over lists (values, or index and value), over tables (key and value), and over inclusive ranges such as `1..10`
- arithmetic
- interned strings, including concatenation, `substring`, `lower`, `upper`, and `string()`/`int()`/`float()` conversions
- typed dynamic lists (`T[]`) with literals, indexing, slices and all list methods
- hashed mixed-key/mixed-value tables: reads and writes through `t[k]` and `t.name`, compound assignment, nesting, and `t.f(args)` calls
- `any` values: checked conversion to typed values, Lua truthiness, and field access and method calls on class objects through generated type info
- closures with Lua upvalue semantics (assigned captures are shared), nested closures, and block closures that return values
- `struct` value types, `static` methods, function and constructor overloading, the native math types (`Vec2`, `Vec3`, `Vec2i`, `Vec3i`, `Quat`, `Mat4x4`) and the `math` library (see below)

The shared runtime uses a precise, non-moving mark-and-sweep heap. It traces module globals, class fields, list elements, table contents, closure environments and cells, and explicit host pins.

These features fail with an explicit backend error:
- a class that derives from an external type that is not an extern class
- conversions between `table` and list values other than literals

Semantic analysis accepts them, so they type-check in the editor.

The C helpers that every generated module uses live in [`runtime/luasharp/runtime.h`](runtime/luasharp/runtime.h). Generated C includes this header. The build copies it to `tcc\include\luasharp\`, where TinyCC finds it.

The lexer produces a source-located token stream and recoverable diagnostics. The parser builds an error-tolerant syntax tree, and semantic analysis produces a separate bound tree containing resolved expression types, symbols, children, and lowering operations. The transpiler consumes that bound tree directly. Both trees store their nodes in flat pools (expressions and statements) that link by index. The parser adds child nodes before their parent and records child ids in `SyntaxTree::links`. Types are interned in `Analysis::types` and nodes refer to them by `TypeId`. Semantic analysis binds scoped locals and parameters, resolves inherited members, validates calls/operators/assignment targets/control-flow conditions/access/overrides, warns about unreachable code, and blocks C emission when errors remain.

The primitive types are `int`, `float`, `string`, `table`, and `bool`. `T[]` is a typed, zero-based list with negative indexing, slicing, and `push_back`, `pop_back`, `size`, `empty`, and `clear`. Strings expose `length`/`size`, `contains`, `starts_with`, `ends_with`, `substring`, `lower`, and `upper`. Primitive conversion uses ordinary call syntax such as `int(value)`, `float(value)`, `bool(value)`, and `string(value)`.

## Structs, static methods and overloading

A `struct` is a value type. It is copied on assignment, on argument passing and on return, and it lives on the stack or inline in its container. It is never garbage collected on its own.

```
struct Particle {
    Vec3 position;
    float mass = 2;
    Particle(Vec3 p) { position = p; }
    Particle(Vec3 p, float m) { position = p; mass = m; }
    void push(Vec3 d) { position = position + d; }
    static Particle origin() { return Particle(Vec3(0, 0, 0)); }
}
Particle a = Particle(Vec3(1, 2, 3));
Particle b = a;          // a copy
b.position.x = 9;        // a is unchanged
a.push(Vec3(1, 0, 0));   // changes a in place
```

- Construct a struct with `T(args)`, `new T(args)` or `T.new(args)` for the native types. All three give a value. `T()` uses the field initializers.
- Fields can have any type, including `string`, lists, `table` and classes. A copy of a struct shares the objects its fields refer to. A struct cannot contain itself, but it can hold a list of itself.
- Structs have no base type. Lambdas in a struct method cannot use `this` or its members.
- A captured struct local lives in a shared cell, so the lambda and the enclosing scope see the same value.
- You can only assign a member of a struct that names storage (a variable, a field, or a list element). `makeParticle().mass = 1` is an error.
- A struct that becomes `any` (or goes into a `table`) is boxed as a copy on the heap. Converting it back copies it out and checks the type. You cannot assign a field of a boxed struct: `t.p.x = 1` is a run-time error. Unbox it, change it and store it again.
- `==` and `!=` compare structs member by member: numbers by value, references by identity. Strings are interned, so equal strings compare equal. Boxed structs compare the same way, and equal structs are the same `table` key.

`static` methods and fields belong to the type: `Util.twice(3)`, `Counter.count += 1`. Inside the type, use the name without the type. A static method or a static field initializer cannot use `this` or instance members.

```
class Counter {
    static int count = 0;
    int id;
    Counter() { count += 1; id = count; }
}
```

- Static fields live in module state. Structs and classes can both have them. A struct's static fields are not part of its value.
- Static fields initialize before top-level code, in declaration order.
- The host reads a static field as the global `Type.field`.

Methods, constructors and static methods can be overloaded. The compiler chooses the overload at compile time. An exact type match is best. After that it prefers `int` to `float`, a derived class to its base class, `null`, and last a conversion through `any`. If two overloads match equally well, the call is an error, unless an argument has the type `any`. The first overload of a name keeps the plain C name (`ls_Util_twice`). Later overloads get a suffix (`ls_Util_twice__1`). Their export names include the parameter types (`Util.twice(float)`). Overloaded methods are not reachable through `any`.

## Inout parameters

An `inout` parameter copies the argument in when the call starts. When the function returns, the call writes the final value back to the argument. There are no references: the function changes only its copy, so the argument does not change until the return. Use `inout` to return several values, or to change a value in place.

```
class Util {
    static void divmod(int a, int b, inout int q, inout int r) { q = 0; while (a >= b) { a -= b; q += 1; } r = a; }
    static void scale(inout Vec3 v, float k) { v = v * k; }
}
int q; int r;
Util.divmod(17, 5, inout q, inout r);   // q is 3, r is 2
Util.scale(inout particles[i].pos, 2);
```

- Write `inout` before the parameter and before the argument. An inout parameter takes only an inout argument, and a value parameter takes no inout argument. Thus `f(int)` and `f(inout int)` are different overloads.
- The argument must be storage: a local, a parameter, a field, a static field, a list element, a struct member of storage, or an extern-class field. A temporary, a constant, a method, or a `table` or `any` entry is an error.
- The argument type must be the same as the parameter type.
- The call evaluates all arguments from left to right before the function starts. It evaluates a list and its index one time. After the return, it writes the copies back in argument order. If you pass the same storage two times, the last write stays. A list element is found again for the write, so the function can add items to that list.
- If the call stops with a runtime error, there is no write back.
- A lambda cannot capture an inout parameter.
- Extern members, lambdas, builtin functions and calls through `any` cannot have inout parameters. `any` code cannot call a method that has inout parameters.
- In C, an inout parameter is a pointer to the caller's copy (`ls_int* x`). Export names spell it `inout int`.

## Inheritance

A class can derive from one other class. Methods are not virtual unless you mark them, as in C#.

```
abstract class Shape {
    string name;
    Shape(string n) { name = n; }
    abstract float area();
    virtual string describe() { return name + ": " + string(area()); }
}
class Rect : Shape {
    float w; float h;
    Rect(float w, float h) : base("rect") { this.w = w; this.h = h; }
    override float area() { return w * h; }
    override string describe() { return "[" + base.describe() + "]"; }
}
sealed class Square : Rect {
    Square(float s) : base(s, s) {}
}
Shape s = new Square(2);
bool isRect = s is Rect;   // true
Rect r = s as Rect;        // null when s is not a Rect
```

- `virtual` lets a derived class replace a method. `override` replaces it, with the same parameters and result. `abstract` declares a method without a body, in an `abstract` class. `sealed override` stops further overrides, and a `sealed` class cannot be a base.
- A method with the same parameters as an inherited one must be an `override`. Hiding an inherited method is an error.
- `Derived(...) : base(args)` calls a base constructor. Without it, the parameterless base constructor runs, and it is an error if the base has none. The base constructor arguments can use the parameters, but not `this`.
- Order of construction: base field initializers, base constructor body, derived field initializers, derived constructor body. A virtual call in a base constructor calls the derived override.
- `base.m(args)` calls the base implementation directly. `base` is not reserved: a field or local called `base` keeps its name.
- `x is T` is true when `x` is not null and is a `T`. `x as T` gives `x` as a `T`, or `null`. Both work on class values and on `any`. On `any`, `is` also tests for `int`, `float`, `bool`, `string`, `table`, lists, enums and structs; `as` needs a reference type.
- A derived class cannot declare a field with the name of an inherited field.

## Extern classes

An `extern class` declares a class that the host program implements. Script code uses it like other classes:

```
extern class Entity {
    int health;
    Entity(int health);
    virtual int damage(int amount);
    static int count();
}
Entity e = new Entity(10);
e.health += 5;
int dealt = e.damage(3);
```

- Members have no bodies, and fields have no initializers. An extern class has at most one constructor and no overloads.
- An extern class can derive only from another extern class.
- A script class can derive from an extern class and override its `virtual` methods. The host calls the overrides, and `base.m()` calls the host implementation. The host makes these objects, so `new` in script is an error and constructors have no parameters. See [native-abi.md](docs/native-abi.md#script-classes-that-derive-from-extern-classes).
- In such a class, `[Editable]` before an instance field shows the field in the editor and serializes it: `[Editable] float speed = 3;`. The field type must be `bool`, `int`, `float`, `string`, `Vec2`, `Vec3`, `Quat` or an extern class. The engine accepts asset classes (`Texture`, `Model`) and `Entity` for extern-class fields.
- `[Editor("Name")]` picks a host editor for an editable field, and `[Former("old")]` gives its previous name so that saved data still loads: `[Editable, Editor("BoneNameString")] string bone;`.
- When the host destroys the object, every script reference to it becomes `null`. A call on a destroyed object reports a runtime error.
- The host supplies one C function for each member. See [native-abi.md](docs/native-abi.md#extern-classes).
- The CsRemake engine links `LuaSharpLib.vcxproj` and generates its declarations and thunks. See [native-abi.md](docs/native-abi.md#engine-binding).

## Projects (`luasharp.json`)

A `luasharp.json` file marks the root of a script project. It names the declaration files of the host:

```json
{
	"declarations": [".luasharp"]
}
```

- Each entry is a `.ls` file or a folder of `.ls` files. A path is relative to `luasharp.json`.
- A `.ls` file belongs to the nearest `luasharp.json` in its folder or above it.
- The LSP gives each file the declarations of its project and the types of the other files in that project. Files in different projects do not see each other. Files outside a project see the other files outside a project.
- The LSP reads the declaration files from disk and reads them again when they change. Go to definition opens them.
- `check` and `emit-c` use the declarations of the project. `run` does not, because a standalone run has no host to supply the declared functions.
- A missing declaration path is a warning.
- The CsRemake editor writes `<scripts>/.luasharp/engine.ls` at startup, and `luasharp.json` if it is missing. Without the editor, use `App.exe --dump-luasharp-decls <scripts folder>`. `.luasharp/` is in `.gitignore`.

## Native math types and the `math` library

`Vec2`, `Vec3` (float32 components), `Vec2i`, `Vec3i` (int32 components), `Quat` and `Mat4x4` are native structs with the layouts in [`runtime/luasharp/vecmath.h`](runtime/luasharp/vecmath.h). `Mat4x4` is column-major like glm: element (row r, column c) is `m[c * 4 + r]`. `Quat` stores `x, y, z, w` like glm, but `Quat.new` takes `(w, x, y, z)`. Euler angles are the glm convention (`pitch, yaw, roll` in `x, y, z`). The reference is in [`docs/math-library.md`](docs/math-library.md).

`math` is a static class with overloaded functions: `math.min(1, 2)` returns an `int`, `math.min(1.5, 2)` a `float`, and `math.min(a, b)` works for `Vec2`, `Vec3`, `Vec2i` and `Vec3i`. Operators are defined per type (`+ - * /`, unary `-`, `==`, and scalar and matrix products).

The vector, matrix and `math` sources are generated. Edit `scripts/gen_vecmath.py` and run `py LuaSharp/scripts/gen_vecmath.py`. It writes `runtime/luasharp/vecmath.h` and the registry rows in `src/BuiltinMath*.inc`.

`table` is the dynamic Lua-table type: arbitrary key and value types are accepted and reads have the internal dynamic type `any`. `Vec3` and `Quat` are native language types whose constructors, methods, fields, and operators extend `vec_quat_lua_stubs.lua`. These native definitions and typed default values live in one declarative registry. All other project and engine types must come from source declarations; the analyzer contains no engine-specific type or function exceptions.

`printf(format, values...)` writes without automatically appending a newline. Native formatting supports `%d`/`%i` for `int`, `%f` for numeric values, `%s` for strings, `%b` for booleans, `%p` for managed references, and `%%` for a literal percent sign. Use `\n` in the format when a newline is wanted.

The VS Code extension indexes every `.ls` file in the workspace and refreshes that index as files change. Cross-file types and members participate in diagnostics, completion, hover, definition, references, rename, and document/workspace symbols. Semantic tokens distinguish user-declared class types from grammar-colored built-in types, and protocol positions are UTF-16 correct.

Remaining features that need an explicit language design are explicit imports/modules, user-defined operator declarations, tuple/multiple-return types, and explicit nullable-type syntax. Formatting and diagnostic code actions are also not implemented yet.

`LuaSharpTests` is the unit-test executable. The native LSP integration test is self-contained and can be run after building `LuaSharpLsp`:

```powershell
node LuaSharp/test/lsp.integration.test.mjs
node LuaSharp/test/cli.integration.test.mjs
```
