#include <gtest/gtest.h>
#include "Framework/ClassBase.h"
#include "Framework/SerializerJson2.h"
#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "Scripting/LuaSharpDeclarations.h"
#include "../../LuaSharp/src/CBackend.h"
#include "Testheader.h"
#include <filesystem>
#include <fstream>
#include <iterator>

// LuaSharp modules calling reflected engine classes through the generated thunks (LsTestThing in Testheader.h).

namespace {
struct Closure
{
	void (*invoke)(void* environment, const luasharp::NativeValue* arguments, luasharp::NativeValue* result);
	void* environment;
};

template <typename T> T global(const luasharp::NativeModule& module, std::string_view name) {
	const auto* address = static_cast<const T*>(module.FindGlobal(name));
	EXPECT_NE(address, nullptr) << name;
	return address ? *address : T{};
}
std::string_view text(const luasharp::NativeModule& module, std::string_view name) {
	return lsbind::view(global<const void*>(module, name));
}
luasharp::NativeValue call(const luasharp::NativeModule& module, std::string_view name) {
	const auto* closure = global<const Closure*>(module, name);
	luasharp::NativeValue result;
	if (closure)
		closure->invoke(closure->environment, nullptr, &result);
	return result;
}
} // namespace

// The tools read the declarations from <scripts>/.luasharp/engine.ls through luasharp.json.
TEST(LuaSharpDeclarations, WritesDeclarationsAndKeepsProjectFile) {
	namespace fs = std::filesystem;
	const auto read = [](const fs::path& path) {
		std::ifstream file(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(file), {});
	};
	const fs::path scripts = fs::temp_directory_path() / "luasharp-declarations-test";
	fs::remove_all(scripts);

	ASSERT_TRUE(write_luasharp_declarations(scripts));
	const std::string declarations = read(scripts / ".luasharp" / "engine.ls");
	EXPECT_NE(declarations.find(lsbind::declaration_source()), std::string::npos);
	EXPECT_NE(declarations.find("extern class Entity : BaseUpdater"), std::string::npos);
	EXPECT_NE(read(scripts / "luasharp.json").find("\".luasharp\""), std::string::npos);

	const std::string custom = "{ \"declarations\": [\".luasharp\", \"extra.ls\"] }";
	std::ofstream(scripts / "luasharp.json", std::ios::binary) << custom;
	std::ofstream(scripts / ".luasharp" / "engine.ls", std::ios::binary) << "stale";
	ASSERT_TRUE(write_luasharp_declarations(scripts));
	EXPECT_EQ(read(scripts / ".luasharp" / "engine.ls"), declarations);
	EXPECT_EQ(read(scripts / "luasharp.json"), custom); // the user owns the project file
	fs::remove_all(scripts);
}

class LuaSharpEngineTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite() { ClassBase::init_classes_startup(); } // create() finds engine classes by name
	void SetUp() override {
		if (!luasharp::NativeModule::IsLibTccAvailable())
			GTEST_SKIP() << "libtcc.dll is not deployed";
		engine = std::make_unique<LuaSharpEngine>();
	}
	void TearDown() override { LsTestThing::first = nullptr; }

	std::unique_ptr<LuaSharpEngine> engine;
};

TEST_F(LuaSharpEngineTest, DeclarationsCompile) {
	EXPECT_NE(engine->declarations().find("extern class Entity : BaseUpdater"), std::string::npos);
	auto module = engine->compile("int x = 1;");
	EXPECT_EQ(global<int64_t>(module, "x"), 1);
}

TEST_F(LuaSharpEngineTest, MethodsFieldsAndValues) {
	LsTestThing thing, partner;
	thing.partner = &partner;
	LsTestThing::first = &thing;
	auto module = engine->compile(R"(
LsTestThing t = LsTestThing.find_first();
int sum = t.add(2, 3);
float scaled = t.scale(2.0);
Vec3 shifted = t.shift(Vec3.new(1, 1, 1));
Quat turned = t.turn(Quat.new(1, 2, 3, 4));
Mat4x4 doubled = t.twice(Mat4x4.identity());
string greeting = t.greet("bob");
string tagged = t.tag("abc");
LsTestMode next = t.next_mode(LsTestMode.Off);
LsTestMode mode0 = t.mode;
t.mode = LsTestMode.Fast;
float speed0 = t.speed;
t.speed = 4.0;
bool enabled0 = t.enabled;
t.enabled = false;
string label0 = t.label;
t.label = "renamed";
Vec3 offset0 = t.offset;
int secret0 = t.secret;
t.secret = 12;
int baseValue = t.base_value;
int described = t.describe();
LsTestBase asBase = t;
int describedBase = asBase.describe();
bool isThing = asBase is LsTestThing;
int count = LsTestThing.instance_count();
LsTestThing p = t.get_partner();
bool partnerOk = t.is_partner(p);
bool selfNotPartner = t.is_partner(t);
bool sameProxy = p == t.get_partner();
t.set_partner(t);
int readOnly = t.read_only;
)");
	EXPECT_EQ(module.LastError(), "");
	EXPECT_EQ(global<int64_t>(module, "sum"), 5);
	EXPECT_DOUBLE_EQ(global<double>(module, "scaled"), 3.0);
	const auto shifted = global<lsbind::Vec3>(module, "shifted");
	EXPECT_EQ(glm::vec3(shifted.x, shifted.y, shifted.z), glm::vec3(2, 3, 4));
	const auto turned = global<lsbind::Quat>(module, "turned");
	EXPECT_EQ(glm::vec4(turned.x, turned.y, turned.z, turned.w), glm::vec4(3, 2, 4, 1));
	const auto doubled = global<lsbind::Mat4>(module, "doubled");
	EXPECT_EQ(lsbind::to_glm(doubled), glm::mat4(2.0f));
	EXPECT_EQ(text(module, "greeting"), "hi bob");
	EXPECT_EQ(text(module, "tagged"), "abc");
	EXPECT_EQ(global<int64_t>(module, "next"), 1);	 // Slow
	EXPECT_EQ(global<int64_t>(module, "mode0"), 1); // Slow
	EXPECT_EQ(thing.mode, LsTestMode::Fast);
	EXPECT_DOUBLE_EQ(global<double>(module, "speed0"), 1.5);
	EXPECT_EQ(thing.speed, 4.0f);
	EXPECT_EQ(global<uint8_t>(module, "enabled0"), 1);
	EXPECT_FALSE(thing.enabled);
	EXPECT_EQ(text(module, "label0"), "thing");
	EXPECT_EQ(thing.label, "renamed");
	const auto offset = global<lsbind::Vec3>(module, "offset0");
	EXPECT_EQ(glm::vec3(offset.x, offset.y, offset.z), glm::vec3(1, 2, 3));
	EXPECT_EQ(global<int64_t>(module, "secret0"), 11);
	EXPECT_EQ(thing.get_secret(), 12);
	EXPECT_EQ(global<int64_t>(module, "baseValue"), 7);
	EXPECT_EQ(global<int64_t>(module, "described"), 2);
	EXPECT_EQ(global<int64_t>(module, "describedBase"), 2);
	EXPECT_EQ(global<uint8_t>(module, "isThing"), 1);
	EXPECT_EQ(global<int64_t>(module, "count"), 3);
	EXPECT_EQ(global<uint8_t>(module, "partnerOk"), 1);
	EXPECT_EQ(global<uint8_t>(module, "selfNotPartner"), 0);
	EXPECT_EQ(global<uint8_t>(module, "sameProxy"), 1);
	EXPECT_EQ(thing.partner, &thing);
	EXPECT_EQ(global<int64_t>(module, "readOnly"), 4);
	EXPECT_EQ(engine->proxy_of(&thing), global<void*>(module, "t"));
}

TEST_F(LuaSharpEngineTest, GenericMethodsAndTypeOf) {
	EXPECT_NE(engine->declarations().find("T get_component<T : Component>();"), std::string::npos);
	EXPECT_NE(engine->declarations().find("[TypeOf] static ClassTypeInfo find(string classname);"), std::string::npos);
	LsTestThing thing, partner;
	thing.partner = &partner;
	LsTestThing::first = &thing;
	auto module = engine->compile(R"(
LsTestThing t = LsTestThing.find_first();
LsTestThing p = t.partner_as<LsTestThing>();
bool viaBase = t.partner_as<LsTestBase>() != null;
LsTestBase raw = t.partner_as(typeof(LsTestThing));
string name = typeof(LsTestThing).get_classname();
bool sameInfo = typeof(LsTestThing) == t.my_type();
)");
	EXPECT_EQ(module.LastError(), "");
	EXPECT_EQ(global<void*>(module, "p"), engine->proxy_of(&partner));
	EXPECT_EQ(global<uint8_t>(module, "viaBase"), 1);
	EXPECT_EQ(global<void*>(module, "raw"), engine->proxy_of(&partner));
	EXPECT_EQ(text(module, "name"), "LsTestThing");
	EXPECT_EQ(global<uint8_t>(module, "sameInfo"), 1);
}

TEST_F(LuaSharpEngineTest, ReadOnlyFieldReportsError) {
	LsTestThing thing;
	LsTestThing::first = &thing;
	try {
		engine->compile("LsTestThing t = LsTestThing.find_first();\nt.read_only = 5;");
		ADD_FAILURE() << "module init did not report the error";
	} catch (const std::runtime_error& error) {
		EXPECT_NE(std::string_view(error.what()).find("read-only"), std::string_view::npos) << error.what();
	}
	EXPECT_EQ(thing.read_only, 4);
}

TEST_F(LuaSharpEngineTest, DeletedObjectIsNull) {
	auto thing = std::make_unique<LsTestThing>();
	LsTestThing::first = thing.get();
	auto module = engine->compile(R"(
LsTestThing held = LsTestThing.find_first();
auto isNull = () => held == null;
auto add = () => held.add(1, 2);
)");
	EXPECT_EQ(call(module, "isNull").as.boolean, 0u);
	EXPECT_EQ(call(module, "add").as.integer, 3);
	engine->collect();
	EXPECT_EQ(call(module, "add").as.integer, 3); // the root slot kept the proxy

	LsTestThing::first = nullptr;
	thing.reset();
	EXPECT_EQ(call(module, "isNull").as.boolean, 1u);
	EXPECT_EQ(module.LastError(), "");
	EXPECT_EQ(call(module, "add").as.integer, 0);
	EXPECT_NE(module.LastError().find("destroyed or null"), std::string::npos) << module.LastError();
}

TEST_F(LuaSharpEngineTest, ShutdownReleasesProxies) {
	LsTestThing thing;
	void* first = engine->proxy_of(&thing);
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(engine->proxy_of(&thing), first);
	engine.reset(); // clears thing's proxy slot; the old runtime is gone
	engine = std::make_unique<LuaSharpEngine>();
	void* second = engine->proxy_of(&thing);
	EXPECT_EQ(luasharp::NativeRuntime::NativeOf(second), &thing);
}

TEST_F(LuaSharpEngineTest, ProxyTypeIsNearestBoundClass) {
	LsTestThing thing;
	LsTestThing::first = &thing;
	auto module = engine->compile(R"(
ClassBase base = LsTestThing.find_first();
bool isThing = base is LsTestThing;
bool isBase = base is LsTestBase;
)");
	EXPECT_EQ(global<uint8_t>(module, "isThing"), 1);
	EXPECT_EQ(global<uint8_t>(module, "isBase"), 1);
}

namespace {
constexpr const char* DerivedSource = R"(
class Spinner : LsTestScriptable {
	int extra = 5;
	int made;
	Spinner() { made = hp + base_value; }
	override int tick(int dt) { return base.tick(dt * 2) + extra; }
	override string label_for(string prefix) { return prefix + "script"; }
	override void notify(LsTestThing other) { other.speed = 9.0; }
	override LsTestThing pick() { return LsTestThing.find_first(); }
	override int describe() { return 30 + base.describe(); }
	int get_made() { return made; }
}
class Fast : Spinner { override int tick(int dt) { return 1; } }
class Plain : LsTestScriptable { int extra = 1; }
class PlainThing : LsTestThing { int extra = 1; }
class BadThing : LsTestThing { override int describe() { return 0; } }
LsTestScriptable held;
auto heldNull = () => held == null;
auto heldIsSpinner = () => held is Spinner;
)";
} // namespace

TEST_F(LuaSharpEngineTest, DerivedClassOverridesVirtuals) {
	LsTestThing thing;
	LsTestThing::first = &thing;
	auto module = engine->compile(DerivedSource);
	std::unique_ptr<ClassBase> object = engine->create(module, "Spinner");
	auto* spinner = dynamic_cast<LsTestScriptable*>(object.get());
	ASSERT_NE(spinner, nullptr);
	void* script = engine->proxy_of(spinner);
	EXPECT_EQ(luasharp::NativeRuntime::NativeOf(script), static_cast<ClassBase*>(spinner));
	using Method = int64_t (*)(void*);
	EXPECT_EQ(reinterpret_cast<Method>(module.FindSymbol("ls_Spinner_get_made"))(script), 107); // the constructor read engine fields

	EXPECT_EQ(spinner->tick(3), 99); // base.tick(6) leaves 94, plus extra
	EXPECT_EQ(spinner->hp, 94);
	EXPECT_EQ(spinner->call_tick(1), 97);
	EXPECT_EQ(spinner->label_for("x"), "xscript");
	spinner->notify(&thing);
	EXPECT_EQ(thing.speed, 9.0f);
	EXPECT_EQ(spinner->last, nullptr); // the override replaced the C++ function
	EXPECT_EQ(spinner->pick(), &thing);
	EXPECT_EQ(spinner->describe(), 31);
	EXPECT_EQ(module.LastError(), "");

	*static_cast<void**>(module.FindGlobal("held")) = script;
	EXPECT_EQ(call(module, "heldIsSpinner").as.boolean, 1u);
	EXPECT_EQ(call(module, "heldNull").as.boolean, 0u);
	object.reset();
	EXPECT_EQ(call(module, "heldNull").as.boolean, 1u); // ~ClassBase destroyed the script object
	EXPECT_EQ(call(module, "heldIsSpinner").as.boolean, 0u);
	engine->collect();
}

TEST_F(LuaSharpEngineTest, DerivedClassInheritsOverrides) {
	auto module = engine->compile(DerivedSource);
	auto fast = engine->create(module, "Fast");
	auto* scriptable = dynamic_cast<LsTestScriptable*>(fast.get());
	ASSERT_NE(scriptable, nullptr);
	EXPECT_EQ(scriptable->tick(3), 1);
	EXPECT_EQ(scriptable->describe(), 31); // from Spinner
	EXPECT_EQ(scriptable->label_for("y"), "yscript");

	auto plain = engine->create(module, "Plain");
	auto* native = dynamic_cast<LsTestScriptable*>(plain.get());
	ASSERT_NE(native, nullptr);
	EXPECT_EQ(native->tick(3), 97); // no overrides: the C++ functions run
	EXPECT_EQ(native->label_for("z"), "znative");
	EXPECT_EQ(native->describe(), 1);
}

TEST_F(LuaSharpEngineTest, DerivedClassOfClassThatIsNotScriptable) {
	auto module = engine->compile(DerivedSource);
	auto plain = engine->create(module, "PlainThing");
	ASSERT_NE(dynamic_cast<LsTestThing*>(plain.get()), nullptr);
	EXPECT_EQ(luasharp::NativeRuntime::NativeOf(engine->proxy_of(plain.get())), plain.get());
	try {
		static_cast<void>(engine->create(module, "BadThing"));
		ADD_FAILURE() << "an override of a class that is not scriptable was accepted";
	} catch (const std::runtime_error& error) {
		EXPECT_NE(std::string_view(error.what()).find("not scriptable"), std::string_view::npos) << error.what();
	}
	EXPECT_THROW(static_cast<void>(engine->create(module, "NoSuchClass")), std::runtime_error);
}

namespace {
constexpr const char* EditableSource = R"(
class Turret : LsTestScriptable {
	[Editable] float range = 2.5;
	[Editable, Former("rounds")] int ammo = 3;
	[Editable] bool armed = true;
	[Editable] string label = "turret";
	[Editable] Vec3 aim = Vec3.new(1, 2, 3);
	[Editable] Quat facing;
	[Editable] Vec2 size;
	int hidden = 4;
	float get_range() { return range; }
	string get_label() { return label; }
	override int tick(int dt) { return ammo; }
}
class HeavyTurret : Turret { [Editable] int armor = 9; }
abstract class Mount : LsTestScriptable { [Editable] int level = 1; }
)";

const PropertyInfo* find_prop(const ClassTypeInfo& type, std::string_view name) {
	if (!type.props)
		return nullptr;
	for (int i = 0; i < type.props->count; i++)
		if (type.props->list[i].name == name)
			return &type.props->list[i];
	return nullptr;
}
template <typename T> T get(const ClassTypeInfo& type, ClassBase& object, std::string_view name) {
	const PropertyInfo* prop = find_prop(type, name);
	EXPECT_NE(prop, nullptr) << name;
	return prop ? prop->get<T>(&object) : T{};
}
template <typename T> void set(const ClassTypeInfo& type, ClassBase& object, std::string_view name, const T& value) {
	const PropertyInfo* prop = find_prop(type, name);
	EXPECT_NE(prop, nullptr) << name;
	if (prop)
		prop->set(&object, value);
}
} // namespace

TEST_F(LuaSharpEngineTest, LoadRegistersScriptClasses) {
	engine->load("turrets", EditableSource);
	const ClassTypeInfo* turret = ClassBase::find_class("Turret");
	const ClassTypeInfo* heavy = ClassBase::find_class("HeavyTurret");
	const ClassTypeInfo* mount = ClassBase::find_class("Mount");
	ASSERT_TRUE(turret && heavy && mount);
	EXPECT_EQ(turret->super_typeinfo, &LsTestScriptable::StaticType);
	EXPECT_EQ(heavy->super_typeinfo, turret);
	EXPECT_TRUE(heavy->is_a(LsTestScriptable::StaticType));
	EXPECT_FALSE(mount->has_allocate_func());

	std::unique_ptr<LsTestScriptable> object(ClassBase::create_class<LsTestScriptable>("HeavyTurret"));
	ASSERT_NE(object, nullptr);
	EXPECT_EQ(&object->get_type(), heavy);
	EXPECT_EQ(object->tick(1), 3); // the override reads the [Editable] field
	EXPECT_EQ(object->hp, 100);

	object.reset();
	engine.reset(); // unregisters the classes
	EXPECT_EQ(ClassBase::find_class("Turret"), nullptr);
	engine = std::make_unique<LuaSharpEngine>();
}

// One module from several files: a script class uses a helper class and an enum of another file.
TEST_F(LuaSharpEngineTest, LoadFilesAsOneModule) {
	const luasharp::SourceFile files[] = {
		{"scripts/a.ls", "class Gun : LsTestScriptable { Ammo ammo = new Ammo(); override int tick(int dt) { return ammo.left(dt); } }\n"
						 "int loaded_a = Ammo.count;"},
		{"scripts/b.ls", "class Ammo { static int count = 4; int left(int dt) { return count - dt; } }\nint loaded_b = 2;"},
	};
	const auto& module = engine->load("scripts", files);
	EXPECT_EQ(engine->find_module("scripts"), &module);
	EXPECT_EQ(*static_cast<int64_t*>(module.FindGlobal("loaded_a")), 4);
	EXPECT_EQ(*static_cast<int64_t*>(module.FindGlobal("loaded_b")), 2);
	std::unique_ptr<LsTestScriptable> gun(ClassBase::create_class<LsTestScriptable>("Gun"));
	ASSERT_NE(gun, nullptr);
	EXPECT_EQ(gun->tick(1), 3);
}

TEST_F(LuaSharpEngineTest, LoadFilesReportsTheFileOfAnError) {
	const luasharp::SourceFile files[] = {{"scripts/a.ls", "class A { }"}, {"scripts/b.ls", "class B { }\nint x = \"text\";"}};
	try {
		engine->load("scripts", files);
		ADD_FAILURE() << "a module with an error loaded";
	} catch (const std::runtime_error& error) {
		EXPECT_TRUE(std::string_view(error.what()).starts_with("scripts/b.ls(2,")) << error.what();
	}
	EXPECT_EQ(engine->find_module("scripts"), nullptr);
}

TEST_F(LuaSharpEngineTest, EditableFieldsArePropertyInfos) {
	const auto& module = engine->load("turrets", EditableSource);
	std::unique_ptr<ClassBase> object = engine->create(module, "Turret");
	const ClassTypeInfo& type = object->get_type();
	ASSERT_EQ(&type, ClassBase::find_class("Turret")); // create() uses the registered type too
	ASSERT_NE(type.props, nullptr);
	EXPECT_EQ(type.props->count, 7);
	EXPECT_EQ(find_prop(type, "hidden"), nullptr);
	EXPECT_EQ(find_prop(type, "range")->type, core_type_id::Double);
	EXPECT_EQ(find_prop(type, "ammo")->type, core_type_id::Int64);
	EXPECT_EQ(find_prop(type, "label")->type, core_type_id::ScriptString);
	EXPECT_EQ(find_prop(type, "size")->type, core_type_id::Vec2);
	EXPECT_TRUE(find_prop(type, "aim")->flags & PROP_SCRIPT_FIELD);

	// Initializers ran; writes through the PropertyInfos reach script code.
	EXPECT_DOUBLE_EQ(get<double>(type, *object, "range"), 2.5);
	EXPECT_EQ(get<int64_t>(type, *object, "ammo"), 3);
	EXPECT_TRUE(get<bool>(type, *object, "armed"));
	EXPECT_EQ(get<glm::vec3>(type, *object, "aim"), glm::vec3(1, 2, 3));
	EXPECT_EQ(get<std::string>(type, *object, "label"), "turret");
	set(type, *object, "range", 7.5);
	set(type, *object, "label", std::string("renamed"));
	void* script = engine->proxy_of(object.get());
	EXPECT_DOUBLE_EQ(reinterpret_cast<double (*)(void*)>(module.FindSymbol("ls_Turret_get_range"))(script), 7.5);
	EXPECT_EQ(lsbind::view(reinterpret_cast<void* (*)(void*)>(module.FindSymbol("ls_Turret_get_label"))(script)), "renamed");
	engine->collect(); // the written string is reachable from the object

	EXPECT_EQ(get<std::string>(type, *object, "label"), "renamed");
	// A derived class lists only its own fields; the editor walks super_typeinfo for the others.
	const ClassTypeInfo* heavy = ClassBase::find_class("HeavyTurret");
	ASSERT_TRUE(heavy && heavy->props);
	EXPECT_EQ(heavy->props->count, 1);
	EXPECT_STREQ(heavy->props->list[0].name, "armor");
}

TEST_F(LuaSharpEngineTest, EditableFieldsAreValues) {
	const auto& module = engine->load("turrets", EditableSource);
	std::unique_ptr<ClassBase> object = engine->create(module, "Turret");
	const ClassTypeInfo& type = object->get_type();
	// A script float and a C++ float are the same domain; so are a GC string and a std::string.
	EXPECT_DOUBLE_EQ(find_prop(type, "range")->get<double>(object.get()), 2.5);
	EXPECT_EQ(find_prop(type, "label")->get<std::string>(object.get()), "turret");
	EXPECT_EQ(find_prop(type, "ammo")->get<int64_t>(object.get()), 3);
	EXPECT_TRUE(find_prop(type, "armed")->get<bool>(object.get()));
	EXPECT_EQ(find_prop(type, "range")->owner, &type);

	find_prop(type, "label")->set(object.get(), std::string("valued"));
	find_prop(type, "aim")->set(object.get(), glm::vec3(7, 8, 9));
	engine->collect();
	EXPECT_EQ(find_prop(type, "label")->get<std::string>(object.get()), "valued");
	EXPECT_EQ(find_prop(type, "aim")->get<glm::vec3>(object.get()), glm::vec3(7, 8, 9));
}

TEST_F(LuaSharpEngineTest, EditableFieldsSerialize) {
	const auto& module = engine->load("turrets", EditableSource);
	std::unique_ptr<ClassBase> source = engine->create(module, "HeavyTurret");
	const ClassTypeInfo& heavy = source->get_type();
	const ClassTypeInfo& turret = *heavy.super_typeinfo;
	set(turret, *source, "range", 0.1);
	set(heavy, *source, "armor", 12);
	set(turret, *source, "aim", glm::vec3(4, 5, 6));
	set(turret, *source, "label", std::string("saved"));

	WriteSerializerBackendJson2 writer("test", *source);
	nlohmann::json json = writer.get_output();
	EXPECT_EQ(json["armor"], 12);
	EXPECT_EQ(json["label"], "saved");

	std::unique_ptr<ClassBase> loaded = engine->create(module, "HeavyTurret");
	ReadSerializerBackendJson2 reader("test", json, *loaded);
	EXPECT_EQ(get<double>(turret, *loaded, "range"), 0.1); // double precision is kept
	EXPECT_EQ(get<int64_t>(heavy, *loaded, "armor"), 12);
	EXPECT_EQ(get<glm::vec3>(turret, *loaded, "aim"), glm::vec3(4, 5, 6));
	EXPECT_EQ(get<std::string>(turret, *loaded, "label"), "saved");
	EXPECT_EQ(get<int64_t>(turret, *loaded, "ammo"), 3);
}

TEST_F(LuaSharpEngineTest, EditableFieldsReadFormerNames) {
	const auto& module = engine->load("turrets", EditableSource);
	std::unique_ptr<ClassBase> object = engine->create(module, "Turret");
	EXPECT_STREQ(find_prop(object->get_type(), "ammo")->former_name, "rounds");
	nlohmann::json json = {{"rounds", 8}};
	ReadSerializerBackendJson2 reader("test", json, *object);
	EXPECT_EQ(get<int64_t>(object->get_type(), *object, "ammo"), 8);
}

TEST_F(LuaSharpEngineTest, EditableEnumFields) {
	const auto& module = engine->load("moods", R"(
enum Mood { Calm = 2, Angry = 7 }
class Moody : LsTestScriptable {
	[Editable] Mood mood = Mood.Angry;
	[Editable] Mood fallback;
	[Editable] LsTestMode mode = LsTestMode.Slow;
	override int tick(int dt) {
		if (mood == Mood.Calm) {
			return 1;
		}
		return 0;
	}
}
)");
	std::unique_ptr<LsTestScriptable> object(static_cast<LsTestScriptable*>(engine->create(module, "Moody").release()));
	const ClassTypeInfo& type = object->get_type();
	const PropertyInfo* mood = find_prop(type, "mood");
	const PropertyInfo* mode = find_prop(type, "mode");
	ASSERT_TRUE(mood && mode);
	ASSERT_EQ(mood->type, core_type_id::Enum32);
	ASSERT_NE(mood->enum_type, nullptr);
	EXPECT_STREQ(mood->enum_type->name, "Mood");
	EXPECT_EQ(mood->enum_type, find_prop(type, "fallback")->enum_type); // one type per enum
	ASSERT_EQ(mood->enum_type->str_count, 2);
	EXPECT_STREQ(mood->enum_type->find_for_value(7)->name, "Angry");
	EXPECT_EQ(EnumRegistry::find_enum_type("Mood"), nullptr); // the class owns it
	// An engine enum field uses the LuaSharp member values: ordinals.
	EXPECT_STREQ(mode->enum_type->name, "LsTestMode");
	EXPECT_STREQ(mode->enum_type->find_for_value(1)->name, "Slow");

	EXPECT_EQ(get<int64_t>(type, *object, "mood"), 7);
	set(type, *object, "mood", int64_t(2));
	EXPECT_EQ(object->tick(1), 1); // the script sees the edit

	WriteSerializerBackendJson2 writer("test", *object);
	nlohmann::json json = writer.get_output();
	std::unique_ptr<ClassBase> loaded = engine->create(module, "Moody");
	ReadSerializerBackendJson2 reader("test", json, *loaded);
	EXPECT_EQ(get<int64_t>(type, *loaded, "mood"), 2);
	EXPECT_EQ(get<int64_t>(type, *loaded, "mode"), 1);
}

TEST_F(LuaSharpEngineTest, ExternClassFieldsAreReferences) {
	const auto& module = engine->load("holders", R"(
class Holder : LsTestScriptable {
	[Editable] IAsset art;
	[Editable, Editor("EntityTarget")] Entity who;
	override int tick(int dt) {
		if (art == null && who == null) {
			return 1;
		}
		return 0;
	}
}
)");
	std::unique_ptr<LsTestScriptable> object(static_cast<LsTestScriptable*>(engine->create(module, "Holder").release()));
	const ClassTypeInfo& type = object->get_type();
	const PropertyInfo* art = find_prop(type, "art");
	const PropertyInfo* who = find_prop(type, "who");
	ASSERT_TRUE(art && who);
	EXPECT_EQ(art->type, core_type_id::AssetPtr);
	EXPECT_EQ(art->class_type, &IAsset::StaticType);
	EXPECT_EQ(who->type, core_type_id::ObjHandlePtr);
	EXPECT_STREQ(who->custom_type_str, "EntityTarget");
	EXPECT_EQ(art->get<AssetRef>(object.get()), (AssetRef{nullptr, &IAsset::StaticType}));
	EXPECT_EQ(who->get<EntityRef>(object.get()).handle, 0);
	EXPECT_EQ(object->tick(0), 1); // the script sees null references

	try {
		engine->load("bad_holder", "class BadHolder : LsTestScriptable { [Editable] LsTestThing thing; }");
		ADD_FAILURE() << "an [Editable] field of a class that is not an asset or an Entity was accepted";
	} catch (const std::runtime_error& error) {
		EXPECT_NE(std::string_view(error.what()).find("an asset or an Entity"), std::string_view::npos) << error.what();
	}
	EXPECT_EQ(ClassBase::find_class("BadHolder"), nullptr);
}

TEST_F(LuaSharpEngineTest, LoadErrors) {
	engine->load("turrets", EditableSource);
	EXPECT_THROW(engine->load("turrets", "int x = 1;"), std::runtime_error); // the name is in use
	try {
		engine->load("again", EditableSource);
		ADD_FAILURE() << "a second class with the same name was registered";
	} catch (const std::runtime_error& error) {
		EXPECT_NE(std::string_view(error.what()).find("already exists"), std::string_view::npos) << error.what();
	}
	try {
		engine->load("bad", "class BadEditable : LsTestThing { [Editable] int x; }");
		ADD_FAILURE() << "[Editable] fields of a class that is not scriptable were accepted";
	} catch (const std::runtime_error& error) {
		EXPECT_NE(std::string_view(error.what()).find("not scriptable"), std::string_view::npos) << error.what();
	}
	EXPECT_EQ(ClassBase::find_class("BadEditable"), nullptr);
	EXPECT_NE(ClassBase::find_class("Turret"), nullptr);
}

// A STRUCT_BODY struct is a LuaSharp struct. Values copy through the thunks field by field.
TEST_F(LuaSharpEngineTest, StructsCrossByValue) {
	EXPECT_NE(engine->declarations().find("extern struct LsTestPoint {\n    Vec3 pos; // = glm::vec3(0, 1, 0)\n    LsTestMode mode; // = LsTestMode::Fast\n    string name; // = \"unnamed\"\n    int[] tags;\n}"), std::string::npos);
	LsTestThing thing;
	LsTestThing::first = &thing;
	thing.point = {glm::vec3(1, 2, 3), LsTestMode::Fast, "start", {1, 2}, 9};
	auto module = engine->compile(R"(
LsTestThing t = LsTestThing.find_first();
LsTestPoint p = t.point;
float x = p.pos.x;
bool fast = p.mode == LsTestMode.Fast;
string name = p.name;
p.pos = Vec3.new(7, 8, 9);
p.name = "moved";
p.mode = LsTestMode.Slow;
t.point = p;
LsTestPoint m = t.moved(p, Vec3.new(1, 0, 0));
float mx = m.pos.x;
int hiddenArg = t.hidden_of(p);
LsTestSegment s = t.segment();
float segX = s.b.pos.x;
LsTestPoint fresh = LsTestPoint();
bool freshDefaults = fresh.mode == LsTestMode.Fast && fresh.pos.y == 1.0 && fresh.name == "unnamed";
bool nestedDefaults = LsTestSegment().b.name == "unnamed";
class Holder { LsTestPoint p; }
string heldName = new Holder().p.name;
)");
	EXPECT_EQ(module.LastError(), "");
	EXPECT_DOUBLE_EQ(global<double>(module, "x"), 1.0);
	EXPECT_EQ(global<uint8_t>(module, "fast"), 1u);
	EXPECT_EQ(text(module, "name"), "start");
	EXPECT_EQ(thing.point.pos, glm::vec3(7, 8, 9));
	EXPECT_EQ(thing.point.name, "moved");
	EXPECT_EQ(thing.point.mode, LsTestMode::Slow);
	EXPECT_EQ(thing.point.hidden, 9); // a field setter assigns in place
	EXPECT_EQ(thing.point.tags, (std::vector<int>{1, 2}));
	EXPECT_DOUBLE_EQ(global<double>(module, "mx"), 8.0);
	EXPECT_EQ(global<int64_t>(module, "hiddenArg"), 5); // an argument starts from T{}
	EXPECT_DOUBLE_EQ(global<double>(module, "segX"), 7.0);
	EXPECT_EQ(global<uint8_t>(module, "freshDefaults"), 1u); // T() is the C++ default value
	EXPECT_EQ(global<uint8_t>(module, "nestedDefaults"), 1u);
	EXPECT_EQ(text(module, "heldName"), "unnamed");
}

TEST_F(LuaSharpEngineTest, ScriptOverrideTakesAndReturnsStruct) {
	auto module = engine->compile(R"(
class Adjuster : LsTestScriptable {
    override LsTestPoint adjust(LsTestPoint p) {
        p.pos = p.pos + Vec3.new(0, 0, 10);
        p.name = p.name + "!";
        return p;
    }
}
)");
	std::unique_ptr<ClassBase> object = engine->create(module, "Adjuster");
	auto* adjuster = dynamic_cast<LsTestScriptable*>(object.get());
	ASSERT_NE(adjuster, nullptr);
	LsTestPoint input{glm::vec3(1, 2, 3), LsTestMode::Fast, "p", {}, 9};
	const LsTestPoint result = adjuster->adjust(input);
	EXPECT_EQ(result.pos, glm::vec3(1, 2, 13));
	EXPECT_EQ(result.name, "p!");
	EXPECT_EQ(result.mode, LsTestMode::Fast);
	EXPECT_EQ(result.hidden, 5); // a slot result starts from T{}
	EXPECT_EQ(module.LastError(), "");
}

// std::vector<T> crosses as a copy in a T[] list.
TEST_F(LuaSharpEngineTest, ListsCrossByCopy) {
	EXPECT_NE(engine->declarations().find("    int[] numbers();\n"), std::string::npos);
	EXPECT_NE(engine->declarations().find("    LsTestPoint[] shifted_points(LsTestPoint[] points);\n"), std::string::npos);
	EXPECT_NE(engine->declarations().find("    int[] tags;\n"), std::string::npos);
	LsTestThing thing, partner;
	thing.partner = &partner;
	LsTestThing::first = &thing;
	thing.point.tags = {4, 5};
	auto module = engine->compile(R"(
LsTestThing t = LsTestThing.find_first();
int[] numbers = t.numbers();
int numberSum = numbers[0] + numbers[1] + numbers[2] + numbers.size();
int sum = t.sum({1, 2, 3, 4});
LsTestThing[] pair = t.pair();
bool pairOk = pair[0] == t && pair[1] == t.get_partner();
string[] words = t.words;
string joined = words[0] + words[1];
words.push_back("c");
t.words = words;
t.path = {Vec3.new(1, 2, 3)};
Vec3[] path = t.path;
float pathZ = path[0].z;
LsTestMode[] modes = t.modes;
bool modesOk = modes[0] == LsTestMode.Off && modes[1] == LsTestMode.Fast;
t.modes = {LsTestMode.Slow};
LsTestPoint p = LsTestPoint();
p.name = "in";
LsTestPoint[] shifted = t.shifted_points({p, p});
float shiftedX = shifted[1].pos.x;
string shiftedName = shifted[0].name;
int tag = t.point.tags[1];
LsTestPoint q = t.point;
q.tags.push_back(6);
t.point = q;
)");
	EXPECT_EQ(module.LastError(), "");
	EXPECT_EQ(global<int64_t>(module, "numberSum"), 9);
	EXPECT_EQ(global<int64_t>(module, "sum"), 10);
	EXPECT_EQ(global<uint8_t>(module, "pairOk"), 1u);
	EXPECT_EQ(text(module, "joined"), "ab");
	EXPECT_EQ(thing.words, (std::vector<std::string>{"a", "b", "c"}));
	EXPECT_EQ(thing.path, (std::vector<glm::vec3>{glm::vec3(1, 2, 3)}));
	EXPECT_DOUBLE_EQ(global<double>(module, "pathZ"), 3.0);
	EXPECT_EQ(global<uint8_t>(module, "modesOk"), 1u);
	EXPECT_EQ(thing.modes, (std::vector<LsTestMode>{LsTestMode::Slow}));
	EXPECT_DOUBLE_EQ(global<double>(module, "shiftedX"), 1.0);
	EXPECT_EQ(text(module, "shiftedName"), "in");
	EXPECT_EQ(global<int64_t>(module, "tag"), 5);
	EXPECT_EQ(thing.point.tags, (std::vector<int>{4, 5, 6}));
	engine->collect();
	EXPECT_EQ(text(module, "joined"), "ab");
	const auto* shifted = static_cast<const lsbind::List*>(global<void*>(module, "shifted"));
	ASSERT_NE(shifted, nullptr);
	EXPECT_EQ(shifted->count, 2);
	EXPECT_EQ(lsbind::view(global<void*>(module, "shiftedName")), "in"); // the GC traced the struct elements
}

// A list is a copy: changing it in script does not change the C++ vector until script assigns it back.
TEST_F(LuaSharpEngineTest, ListFieldIsACopy) {
	LsTestThing thing;
	LsTestThing::first = &thing;
	auto module = engine->compile(R"(
LsTestThing t = LsTestThing.find_first();
t.words.push_back("c");
int count = t.words.size();
)");
	EXPECT_EQ(module.LastError(), "");
	EXPECT_EQ(global<int64_t>(module, "count"), 2);
	EXPECT_EQ(thing.words.size(), 2u);
}

TEST_F(LuaSharpEngineTest, ScriptOverrideTakesAndReturnsList) {
	auto module = engine->compile(R"(
class Lister : LsTestScriptable {
    override int count_all(int[] values) {
        int total = 0;
        for (v in values) { total += v; }
        return total;
    }
    override string[] names() { return {"x", "y"}; }
}
)");
	std::unique_ptr<ClassBase> object = engine->create(module, "Lister");
	auto* lister = dynamic_cast<LsTestScriptable*>(object.get());
	ASSERT_NE(lister, nullptr);
	EXPECT_EQ(lister->count_all({2, 3}), 5);
	EXPECT_EQ(lister->names(), (std::vector<std::string>{"x", "y"}));
	EXPECT_EQ(module.LastError(), "");
}

TEST_F(LuaSharpEngineTest, CompileErrorThrows) {
	EXPECT_THROW(engine->compile("LsTestThing t = new LsTestThing();"), std::runtime_error);
	EXPECT_THROW(engine->compile("int x = LsTestThing.no_such_member();"), std::runtime_error);
}
