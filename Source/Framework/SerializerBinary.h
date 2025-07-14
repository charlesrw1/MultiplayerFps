#pragma once
#include "Serializer.h"
#include "BinaryReadWrite.h"
class BinaryWriterBackend : public Serializer {
public:
	BinaryWriterBackend(ClassBase& rootObj);

	// Inherited via Serializer
	virtual bool serialize_dict(const char* tag) override;

	virtual bool serialize_dict_ar() override;

	virtual bool serialize_array(const char* tag, int& size) override;

	virtual bool serialize_array_ar(int& size) override;

	virtual void end_obj() override;

	virtual bool serialize(const char* tag, bool& b) override;

	virtual bool serialize(const char* tag, int8_t& i) override;

	virtual bool serialize(const char* tag, int16_t& i) override;

	virtual bool serialize(const char* tag, int32_t& i) override;

	virtual bool serialize(const char* tag, int64_t& i) override;

	virtual bool serialize(const char* tag, float& f) override;

	virtual bool serialize(const char* tag, glm::vec3& f) override;

	virtual bool serialize(const char* tag, glm::vec2& f) override;

	virtual bool serialize(const char* tag, glm::quat& f) override;

	virtual bool serialize(const char* tag, std::string& s) override;

	virtual void serialize_ar(bool& b) override;

	virtual void serialize_ar(int8_t& i) override;

	virtual void serialize_ar(int16_t& i) override;

	virtual void serialize_ar(int32_t& i) override;

	virtual void serialize_ar(int64_t& i) override;

	virtual void serialize_ar(float& f) override;

	virtual void serialize_ar(glm::vec3& f) override;

	virtual void serialize_ar(glm::vec2& f) override;

	virtual void serialize_ar(glm::quat& f) override;

	virtual void serialize_ar(std::string& s) override;

	virtual bool serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) override;

	virtual void serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr) override;

	virtual bool serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) override;

	virtual void serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr) override;

	virtual bool serialize_enum(const char* tag, const EnumTypeInfo* info, int& i) override;

	virtual void serialize_enum_ar(const EnumTypeInfo* info, int& i) override;

	virtual bool serialize_asset(const char* tag, const ClassTypeInfo& type, IAsset*& ptr) override;

	virtual void serialize_asset_ar(const ClassTypeInfo& type, IAsset*& ptr) override;

	virtual bool is_loading() override;

	virtual const char* get_debug_tag() override;

	void write_class(ClassBase& b);

	FileWriter writer;
	int curIndex = 0;
	std::unordered_map<ClassBase*, int> classToIndex;
};