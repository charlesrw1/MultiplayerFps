#include "ReflectionProp.h"
#include <json.hpp>
#include "FnFactory.h"
#include "glm/glm.hpp"

using SerializerFactory = FnFactory<IPropertySerializer>;

using json = nlohmann::json;


struct IterateProperties
{
	IterateProperties(ClassBase* obj, bool only_serializable) : obj(obj), only_serializable(only_serializable) {}
	struct Iterator {
		Iterator(ClassBase* obj, bool only_seri) {
			this->obj = obj;
			this->info = &obj->get_type();
			this->only_serializable = only_seri;
			index = -1;
			advance();
		}
		Iterator() {
		}

		bool operator!=(const Iterator& other) {
			return info!=nullptr;
		}
		Iterator& operator++() {
			advance();
			return *this;
		}
		PropertyInfo& operator*() {
			assert(info);
			auto props = info->props;
			assert(props);
			assert(index < props->count);
			return props->list[index];
		}

		bool only_serializable = false;
		int index = 0;
		const ClassTypeInfo* info = nullptr;
		ClassBase* obj = nullptr;

		void advance() {
			++index;
			while (true) {
				if (!info)
					return;
				auto props = info->props;
				if (!props) {
					info = info->super_typeinfo;
					index = 0;
					continue;
				}
				if (index >= props->count) {
					info = info->super_typeinfo;
					index = 0;
					continue;
				}
				if (only_serializable && !props->list[index].can_serialize()) {
					++index;
					continue;
				}
				break;
			}
		}
	};
	Iterator begin() {
		return Iterator(obj, only_serializable);
	}
	Iterator end() {
		return Iterator();
	}
	ClassBase* obj = nullptr;
	bool only_serializable = false;
};


nlohmann::json serialize_objects_json(const std::vector<ClassBase*>& objects, const SerializerFactory& factory)
{
	return {};
}
std::string serialize_objects(const std::vector<ClassBase*>& objects, const SerializerFactory& factory)
{
	return{};
}

std::string serialize_vec3_property(const PropertyInfo& property, ClassBase* object)
{
	glm::vec3* v = (glm::vec3*)property.get_ptr(object);
	return std::to_string(v->x) + " " + std::to_string(v->y) + " " + std::to_string(v->z);
}
nlohmann::json serialize_list_property(const PropertyInfo& property, ClassBase* object, const SerializerFactory& factory)
{
	return "";
}

nlohmann::json serialize_object_json(ClassBase* object, const SerializerFactory& factory);
nlohmann::json serialize_property(const PropertyInfo& property, ClassBase* object, const SerializerFactory& factory)
{
	switch (property.type) {
	case core_type_id::Bool:
		return (bool)property.get_int(object);
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:
		return property.get_int(object);
	case core_type_id::Float:
		return property.get_float(object);
	case core_type_id::Vec3:
		return serialize_vec3_property(property, object); 
	case core_type_id::List:
		return serialize_list_property(property, object, factory);
	case core_type_id::StdString:
		return *(std::string*)property.get_ptr(object);
	case core_type_id::StdUniquePtr:
		break;
	}

	return "";
}

// 

// animation:
//		animator asset
//			vector<AnimationNode*> nodes
//			AnimationNode*			root
//			ClassType*
// 
//			id_map [id, index]
// 
//		
//		serialize editor and anim nodes together...
// 
// level:
//		hashset<Object*> objects
//		



nlohmann::json serialize_object_json(ClassBase* object, const SerializerFactory& factory)
{
	assert(object);
	json output;
	output["class"] = object->get_type().classname;
	for (auto& prop : IterateProperties(object,true)) {
		output[prop.name] = serialize_property(prop, object, factory);
	}
	return output;
}
std::string serialize_object(ClassBase* object, const SerializerFactory& factory)
{
	return serialize_object_json(object, factory).dump();
}
ClassBase* unserialize_object(std::string data, const SerializerFactory& factory)
{
	return {};
}
std::vector<ClassBase*> unserialize_objects(std::string data, const SerializerFactory& factory)
{
	return {};
}

