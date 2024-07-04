#include "Framework/ClassBase.h"

#include <unordered_map>
#include <vector>

#include "Framework/Util.h"

#include "Framework/WriteObject.h"

ClassTypeInfo ClassBase::StaticType = ClassTypeInfo("ClassBase", "", nullptr, nullptr);
const ClassTypeInfo& ClassBase::get_type() const { return ClassBase::StaticType; }

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


ClassTypeInfo::ClassTypeInfo(const char* classname, const char* superclass, const PropertyInfoList* props, CreateObjectFunc alloc)
{
	this->classname = classname;
	this->superclassname = superclass;
	this->props = props;
	this->allocate = alloc;

	// register this
	ClassBase::register_class(this);
}

const ClassTypeInfo* ClassTypeIterator::get_type() const
{
	return ClassBase::find_class(index);
}

void ClassBase::register_class(ClassTypeInfo* cti)
{
	if (get_registry().initialzed)
		Fatalf("!!! RegisterClass called outside of static initialization\n");
	if (*cti->superclassname == 0 /*strlen == 0*/ && cti != &ClassBase::StaticType)
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
	if (strlen(typeinfo->superclassname) > 0) {
		auto super = get_registry().string_to_typeinfo.find(typeinfo->superclassname);
		if (super == get_registry().string_to_typeinfo.end())
			Fatalf("!!! Couldnt find super class %s for class %s\n", typeinfo->superclassname, typeinfo->classname);

		// initialize super class
		if (!super->second.has_initialized)
			super->second.init();

		typeinfo->super_typeinfo = super->second.typeinfo;

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


void ClassBase::init()
{
	// create class tree graph
	for (auto& class_ : get_registry().string_to_typeinfo) {
		class_.second.init();
	}

	// now every class is initialized and a tree exists, set everyones ids
	auto root_class = get_registry().string_to_typeinfo.find(ClassBase::StaticType.classname);
	ASSERT(root_class != get_registry().string_to_typeinfo.end());
	set_typenum_R(&root_class->second);

	sys_print("``` Initialized Classes; num classes: %d\n", (int)get_registry().id_to_typeinfo.size());

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
const ClassTypeInfo* ClassBase::find_class(uint16_t id)
{
	ASSERT(get_registry().initialzed);

	auto& list = get_registry().id_to_typeinfo;

	if (id >= 0 && id < list.size())
		return list[id];
	return nullptr;
}

ClassBase* ClassBase::create_copy(TypedVoidPtr userptr)
{
	ASSERT(get_type().allocate);
	ClassBase* copied = get_type().allocate();
	ASSERT(copied);
	copy_object_properties(this, copied, userptr);
	return copied;
}