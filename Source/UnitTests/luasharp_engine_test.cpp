#include <gtest/gtest.h>
#include "Framework/ClassBase.h"
#include "Framework/SerializerJson2.h"
#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "../../LuaSharp/src/CBackend.h"
#include "Testheader.h"

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
	EXPECT_NE(engine->declarations().find("// skipped numbers(): std::vector<int>"), std::string::npos);
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
	[Editable] int ammo = 3;
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
template <typename T> T& at(const ClassTypeInfo& type, ClassBase& object, std::string_view name) {
	const PropertyInfo* prop = find_prop(type, name);
	EXPECT_NE(prop, nullptr) << name;
	return *reinterpret_cast<T*>(prop->get_ptr(&object));
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
	EXPECT_DOUBLE_EQ(at<double>(type, *object, "range"), 2.5);
	EXPECT_EQ(at<int64_t>(type, *object, "ammo"), 3);
	EXPECT_EQ(at<uint8_t>(type, *object, "armed"), 1);
	EXPECT_EQ(at<glm::vec3>(type, *object, "aim"), glm::vec3(1, 2, 3));
	EXPECT_EQ(find_prop(type, "label")->get_script_string(object.get()), "turret");
	at<double>(type, *object, "range") = 7.5;
	find_prop(type, "label")->set_script_string(object.get(), "renamed");
	void* script = engine->proxy_of(object.get());
	EXPECT_DOUBLE_EQ(reinterpret_cast<double (*)(void*)>(module.FindSymbol("ls_Turret_get_range"))(script), 7.5);
	EXPECT_EQ(lsbind::view(reinterpret_cast<void* (*)(void*)>(module.FindSymbol("ls_Turret_get_label"))(script)), "renamed");
	engine->collect(); // the written string is reachable from the object

	EXPECT_EQ(find_prop(type, "label")->get_script_string(object.get()), "renamed");
	// A derived class lists only its own fields; the editor walks super_typeinfo for the others.
	const ClassTypeInfo* heavy = ClassBase::find_class("HeavyTurret");
	ASSERT_TRUE(heavy && heavy->props);
	EXPECT_EQ(heavy->props->count, 1);
	EXPECT_STREQ(heavy->props->list[0].name, "armor");
}

TEST_F(LuaSharpEngineTest, EditableFieldsSerialize) {
	const auto& module = engine->load("turrets", EditableSource);
	std::unique_ptr<ClassBase> source = engine->create(module, "HeavyTurret");
	const ClassTypeInfo& heavy = source->get_type();
	const ClassTypeInfo& turret = *heavy.super_typeinfo;
	at<double>(turret, *source, "range") = 0.1;
	at<int64_t>(heavy, *source, "armor") = 12;
	at<glm::vec3>(turret, *source, "aim") = glm::vec3(4, 5, 6);
	find_prop(turret, "label")->set_script_string(source.get(), "saved");

	WriteSerializerBackendJson2 writer("test", *source);
	nlohmann::json json = writer.get_output();
	EXPECT_EQ(json["armor"], 12);
	EXPECT_EQ(json["label"], "saved");

	std::unique_ptr<ClassBase> loaded = engine->create(module, "HeavyTurret");
	ReadSerializerBackendJson2 reader("test", json, *loaded);
	EXPECT_EQ(at<double>(turret, *loaded, "range"), 0.1); // double precision is kept
	EXPECT_EQ(at<int64_t>(heavy, *loaded, "armor"), 12);
	EXPECT_EQ(at<glm::vec3>(turret, *loaded, "aim"), glm::vec3(4, 5, 6));
	EXPECT_EQ(find_prop(turret, "label")->get_script_string(loaded.get()), "saved");
	EXPECT_EQ(at<int64_t>(turret, *loaded, "ammo"), 3);
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

TEST_F(LuaSharpEngineTest, CompileErrorThrows) {
	EXPECT_THROW(engine->compile("LsTestThing t = new LsTestThing();"), std::runtime_error);
	EXPECT_THROW(engine->compile("int x = LsTestThing.no_such_member();"), std::runtime_error);
}
