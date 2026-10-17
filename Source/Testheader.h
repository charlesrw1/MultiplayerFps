#pragma once
#include <string>
#include "Framework/ClassBase.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/StringName.h"
#include "Framework/EnumDefReflection.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include "Assets/IAsset.h"
#include "Game/EntityPtr.h"
#include "Framework/Util.h"
#include "Framework/SavedFields.h"

// LuaSharp binding tests (luasharp_engine_test.cpp). Values are not ordinals, so the thunks must map them.
NEWENUM(LsTestMode, uint8_t){
	Off = 2,
	Slow = 5,
	Fast = 9,
};

class LsTestBase : public ClassBase
{
public:
	CLASS_BODY(LsTestBase);
	REF virtual int describe() { return 1; }
	REF int base_value = 7;
};

class LsTestThing : public LsTestBase
{
public:
	CLASS_BODY(LsTestThing);
	REF int describe() override { return 2; } // dispatches through LsTestBase's thunk

	REF float speed = 1.5f;
	REF bool enabled = true;
	REF std::string label = "thing";
	REF LsTestMode mode = LsTestMode::Slow;
	REF glm::vec3 offset = glm::vec3(1, 2, 3);
	REFLECT(script_read)
	int read_only = 4;

	REF int add(int a, int b) { return a + b; }
	REF float scale(float f) { return f * speed; }
	REF glm::vec3 shift(glm::vec3 v) { return v + offset; }
	REF glm::quat turn(glm::quat q) { return glm::quat(q.w, q.y, q.x, q.z); }
	REF glm::mat4 twice(glm::mat4 m) { return m * 2.0f; }
	REF std::string greet(const std::string& name) { return "hi " + name; }
	REF StringName tag(StringName n) { return n; }
	REF LsTestMode next_mode(LsTestMode m) { return m == LsTestMode::Off ? LsTestMode::Slow : LsTestMode::Fast; }
	REF LsTestThing* get_partner() { return partner; }
	REF void set_partner(LsTestThing* p) { partner = p; }
	REF bool is_partner(const LsTestThing* other) { return other == partner; }
	REF int get_secret() { return secret; }
	REF std::vector<int> numbers() { return {}; } // not bound in this milestone
	REF static int instance_count() { return 3; }
	REF static LsTestThing* find_first() { return first; }

	static inline LsTestThing* first = nullptr;
	LsTestThing* partner = nullptr;

private:
	REF int secret = 11; // bound through its PropertyInfo offset
};

// A LuaSharp script class derives from it; C++ virtual calls go to the script overrides.
class LsTestScriptable : public LsTestBase
{
public:
	CLASS_BODY(LsTestScriptable, scriptable);
	REF virtual int tick(int dt) {
		hp -= dt;
		return hp;
	}
	REF virtual std::string label_for(const std::string& prefix) { return prefix + "native"; }
	REF virtual void notify(LsTestThing* other) { last = other; }
	REF virtual LsTestThing* pick() { return nullptr; }
	REF int call_tick(int dt) { return tick(dt); } // a virtual call from C++
	REF int hp = 100;
	LsTestThing* last = nullptr;
};

class ITestInterface {
public:
	INTERFACE_BODY();
	REF virtual int interface_value() { return 42; }
	REF virtual void interface_action() {}
};

class ISecondInterface {
public:
	INTERFACE_BODY();
	REF virtual std::string second_name() { return "default"; }
};

class TestWithInterface : public ClassBase, public ITestInterface
{
public:
	CLASS_BODY(TestWithInterface, scriptable);
	int interface_value() override { return 100; }
	void interface_action() override {}
};

class TestWithTwoInterfaces : public ClassBase, public ITestInterface, public ISecondInterface
{
public:
	CLASS_BODY(TestWithTwoInterfaces);
	int interface_value() override { return 200; }
	void interface_action() override {}
	std::string second_name() override { return "two_ifaces"; }
};

class InterfaceClass : public ClassBase
{
public:
	CLASS_BODY(InterfaceClass, scriptable);

	REF virtual int get_value(std::string str) {
		printf("base get_value\n");

		return 0;
	}
	REF virtual void buzzer() { printf("base buzzer\n"); }
	REF int self_func() {
		printf("self_func\n");

		return variable;
	}
	REF void set_var(int v) {
		printf("set_var %d\n", v);

		variable = v;
	}
	REF void set_str(std::string s) { myStr = s; }

	// StringName Lua/C++ boundary round-trip, exercised by scripting_lua_dispatch_test.cpp.
	REF StringName get_name_field() { return nameField; }
	REF void set_name_field(StringName n) { nameField = n; }
	REF bool name_field_is(std::string s) { return nameField == StringName(s.c_str()); }

	std::string myStr;
	int variable = 0;
	StringName nameField;
};

// Property accessor tests (property_access_test.cpp): one field of each value domain.
class PropAccessTestThing : public ClassBase
{
public:
	CLASS_BODY(PropAccessTestThing);
	REF float speed = 1.5f;
	REFLECT(min = 0, max = 10)
	float clamped = 5.f;
	REF int32_t signed_count = -3;
	REF uint8_t mask = 0xF0;
	REF bool enabled = true;
	REF std::string label = "thing";
	REF StringName tag;
	REF glm::vec3 offset = glm::vec3(1, 2, 3);
	REF glm::quat facing = glm::quat(1, 0, 0, 0);
	REF LsTestMode mode = LsTestMode::Fast;
	REF AssetPtr<IAsset> asset;
	REF obj<Entity> target;
	REF Color32 tint = COLOR_RED;
};

// Saved-data renames and upgrades (property_access_test.cpp). Version 2 renamed "power" to "strength" and doubled it.
class PropUpgradeTestThing : public ClassBase
{
public:
	CLASS_BODY(PropUpgradeTestThing);
	REFLECT(former = health)
	int hp = 10;
	REF int strength = 1;
	int get_data_version() const override { return 2; }
	void upgrade_data(int from_version, SavedFields& fields) override {
		if (from_version >= 2)
			return;
		fields.rename("power", "strength");
		if (fields.has("strength"))
			fields.json()["strength"] = fields.json()["strength"].get<int>() * 2;
	}
};
