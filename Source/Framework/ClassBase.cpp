#include "Framework/ClassBase.h"
#include <unordered_map>
#include <vector>
#include "Framework/Util.h"
#include "PropHashTable.h"
#include "SerializedForDiffing.h"
#include "Scripting/ScriptManager.h"

const bool ClassBase::CreateDefaultObject = false;

bool ClassTypeInfo::is_subclass_of(const ClassTypeInfo* info) const {
	assert(info);
	return is_a(*info);
}

std::string ClassTypeInfo::get_classname() const
{
	return classname;
}

const ClassTypeInfo* ClassTypeInfo::get_super_type() const
{
	return super_typeinfo;
}


int ClassTypeInfo::get_prototype_index_table() const {
	assert(lua_prototype_index_table != 0);
	return lua_prototype_index_table;
}

struct TypeInfoWithExtra
{
	TypeInfoWithExtra(ClassTypeInfo* ti) : typeinfo(ti) {}
	ClassTypeInfo* typeinfo = nullptr;
	TypeInfoWithExtra* child = nullptr;
	TypeInfoWithExtra* next = nullptr;
	bool has_initialized = false;

	void set_parent(TypeInfoWithExtra* owner) {
		ASSERT(owner);
		
		this->next = owner->child;
		owner->child = this;
	}


	void init();
};

struct ClassRegistryData
{
	std::unordered_map <std::string, TypeInfoWithExtra> string_to_typeinfo;
	std::vector<ClassTypeInfo*> id_to_typeinfo;
	bool initialzed = false;
};


static ClassRegistryData& get_registry()
{
	static ClassRegistryData inst;
	return inst;
}


ClassTypeInfo::ClassTypeInfo(const char* classname, const ClassTypeInfo* super_typeinfo, 
	GetPropsFunc_t get_props_func, CreateObjectFunc alloc, bool create_default_obj,
	const FunctionInfo* lua_funcs, int lua_func_count, CreateObjectFunc scriptAlloc, bool is_lua_obj)
{
	this->classname = classname;
	this->superclassname = "";
	this->props = nullptr;
	this->allocate = alloc;
	this->super_typeinfo = super_typeinfo;
	this->get_props_function = get_props_func;

	// this gets fixed up later
	this->default_class_object = (ClassBase*)create_default_obj;
	this->lua_functions = lua_funcs;
	this->lua_function_count = lua_func_count;
	this->scriptable_allocate = scriptAlloc;
	if (is_lua_obj)
		this->default_class_object = nullptr;
	else {
		// register this
		ClassBase::register_class(this);
	}
}

ClassTypeInfo::~ClassTypeInfo()
{
}

ClassTypeIterator::ClassTypeIterator(ClassTypeInfo* ti) {
	if (ti) {
		index = ti->id;
		end = ti->last_child + 1;
	}
}

const ClassTypeInfo* ClassTypeIterator::get_type() const
{
	return ClassBase::find_class(index);
}

const ClassTypeInfo* ClassBase::my_type() const { return &get_type(); }

bool ClassBase::is_subclass_of(const ClassTypeInfo* type) const
{
	assert(type);
	return get_type().is_a(*type);
}

void ClassBase::register_class(ClassTypeInfo* cti)
{
	if (get_registry().initialzed)
		Fatalf("!!! RegisterClass called outside of static initialization\n");
	if (!cti->super_typeinfo && cti != &ClassBase::StaticType)
		Fatalf("!!! RegisterClass called without a super class, parent to ClassBase if its a root class");

	auto& string_to_typeinfo = get_registry().string_to_typeinfo;

	std::string cn_str = cti->classname;
	auto find = string_to_typeinfo.find(cn_str);
	if (find != string_to_typeinfo.end())
		Fatalf("!!! RegisterClass two classes defined for %s", cn_str.c_str());
	string_to_typeinfo.insert({ cn_str,TypeInfoWithExtra(cti) });
}

void TypeInfoWithExtra::init()
{
	ASSERT(typeinfo);

	if (has_initialized)
		return;
	if (typeinfo->super_typeinfo) {
		typeinfo->superclassname = typeinfo->super_typeinfo->classname;

		auto super = get_registry().string_to_typeinfo.find(typeinfo->superclassname);
		if (super == get_registry().string_to_typeinfo.end())
			Fatalf("!!! Couldnt find super class %s for class %s\n", typeinfo->superclassname, typeinfo->classname);

		// initialize super class
		if (!super->second.has_initialized)
			super->second.init();

		set_parent(&super->second);
	}

	has_initialized = true;
}

static void set_typenum_R(TypeInfoWithExtra* node)
{
	node->typeinfo->id = get_registry().id_to_typeinfo.size();
	get_registry().id_to_typeinfo.push_back(node->typeinfo);

	TypeInfoWithExtra* child = node->child;
	while (child) {
		set_typenum_R(child);
		child = child->next;
	}

	uint32_t end = get_registry().id_to_typeinfo.size();
	node->typeinfo->last_child = end - 1;	// last child ID
}


void ClassBase::init_class_reflection_system()
{
	// create class tree graph
	for (auto& class_ : get_registry().string_to_typeinfo) {
		class_.second.init();
	}

	// now every class is initialized and a tree exists, set everyones ids
	auto root_class = get_registry().string_to_typeinfo.find(ClassBase::StaticType.classname);
	ASSERT(root_class != get_registry().string_to_typeinfo.end());
	set_typenum_R(&root_class->second);

	// now call get props functions
	for (auto& classtype : get_registry().id_to_typeinfo) {
		if (classtype->get_props_function) {
			classtype->props = classtype->get_props_function();
			
		}
		PropHashTable* table = new PropHashTable;
		if (classtype->super_typeinfo)
			table->prop_table = classtype->super_typeinfo->prop_hash_table->prop_table;	// copy parents hashtable
		// write the keys
		if (classtype->props) {
			for (int i = 0; i < classtype->props->count; i++) {
				auto& prop = classtype->props->list[i];
				StringView prop_name_as_sv(prop.name);
				table->prop_table.insert({ prop_name_as_sv, &prop });
			}
		}
		classtype->prop_hash_table = table;
	}
	// now call default constructors
	auto& id_to_typeinfo = get_registry().id_to_typeinfo;
	for (auto classtype : id_to_typeinfo) {
		if (classtype->default_class_object != nullptr) {
			if (classtype->has_allocate_func())
				classtype->default_class_object = classtype->allocate_this_type();
			else
				classtype->default_class_object = nullptr;

			if (classtype->default_class_object) {
				MakePathForGenericObj pathmaker(false);
				WriteSerializerBackendJson writer("DiffObj", pathmaker, *(ClassBase*)classtype->default_class_object);
				auto rootOut = writer.get_root_object();
				if (!rootOut) {
					sys_print(Error, "ClassBase::init: couldn't create a diff for class: %s\n", classtype->classname);
				}
				else {
					classtype->diff_data = std::make_unique<SerializedForDiffing>();
					classtype->diff_data->jsonObj = std::move(*rootOut);
				}
			}
		}
	}

	sys_print(Debug, "Initialized Classes; num classes: %d\n", (int)get_registry().id_to_typeinfo.size());

	get_registry().initialzed = true;
}

const ClassTypeInfo* ClassBase::find_class(const char* classname)
{
	ASSERT(get_registry().initialzed);

	auto& string_to_typeinfo = get_registry().string_to_typeinfo;

	std::string cn_str = classname;
	auto find = string_to_typeinfo.find(cn_str);
	if (find != string_to_typeinfo.end())
		return find->second.typeinfo;
	return nullptr;
}
// find a ClassTypeInfo by integer id
const ClassTypeInfo* ClassBase::find_class(int32_t id)
{
	ASSERT(get_registry().initialzed);

	auto& list = get_registry().id_to_typeinfo;

	if (id >= 0 && id < list.size())
		return list[id];
	return nullptr;
}
void ClassBase::init_class_info_for_script()
{
	auto& classes = get_registry().id_to_typeinfo;
	for (auto c : classes) {
		ScriptManager::inst->init_this_class_type(c);
		assert(c->get_prototype_index_table()!=0);
	}
	for (auto c : classes) {
		assert(c->get_prototype_index_table() != 0);
		ScriptManager::inst->set_class_type_global(c);
	}
}
int ClassBase::get_table_registry_id()
{
	if (lua_table_id == 0) {
	//	sys_print(Debug, "ClassBase::get_table_registry_id\n");
		lua_table_id = ScriptManager::inst->create_class_table_for(this);
		assert(is_class_referenced_from_lua());
	}
	return lua_table_id;
}
bool ClassBase::is_class_referenced_from_lua() const
{
	return lua_table_id!=0;
}
#include "Assets/AssetDatabase.h"
ClassBase* ClassBase::create_copy(ClassBase* userptr)
{
	ASSERT(get_type().has_allocate_func());
	ClassBase* copied = get_type().allocate_this_type();
	ASSERT(copied);
	copy_object_properties(this, copied, userptr, AssetDatabase::loader);
	return copied;
}