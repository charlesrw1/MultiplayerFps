#include "RmlUiDataModel.h"

namespace {

class GenericScalarDefinition final : public Rml::VariableDefinition {
public:
	GenericScalarDefinition() : VariableDefinition(Rml::DataVariableType::Scalar) {}
	bool Get(void* ptr, Rml::Variant& variant) override {
		variant = *static_cast<Rml::Variant*>(ptr);
		return true;
	}
	bool Set(void* ptr, const Rml::Variant& variant) override {
		*static_cast<Rml::Variant*>(ptr) = variant;
		return true;
	}
};

class GenericRowDefinition final : public Rml::VariableDefinition {
public:
	GenericRowDefinition() : VariableDefinition(Rml::DataVariableType::Struct) {}
	Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
		RmlGenericRow* row = static_cast<RmlGenericRow*>(ptr);
		// operator[] default-constructs an empty Variant for a field name
		// not yet written by Lua, so `{{ card.text }}` on a not-yet-populated
		// field renders empty instead of erroring.
		Rml::Variant& slot = row->fields[address.name];
		return Rml::DataVariable(rmlui_generic_scalar_definition(), &slot);
	}
};

class GenericArrayDefinition final : public Rml::VariableDefinition {
public:
	GenericArrayDefinition() : VariableDefinition(Rml::DataVariableType::Array) {}
	int Size(void* ptr) override {
		return (int)static_cast<std::vector<RmlGenericRow>*>(ptr)->size();
	}
	Rml::DataVariable Child(void* void_ptr, const Rml::DataAddressEntry& address) override {
		auto* vec = static_cast<std::vector<RmlGenericRow>*>(void_ptr);
		const int size = (int)vec->size();
		if (address.index < 0 || address.index >= size) {
			if (address.name == "size")
				return Rml::MakeLiteralIntVariable(size);
			return Rml::DataVariable();
		}
		return Rml::DataVariable(rmlui_generic_row_definition(), &(*vec)[address.index]);
	}
};

} // namespace

Rml::VariableDefinition* rmlui_generic_scalar_definition() {
	static GenericScalarDefinition def;
	return &def;
}
Rml::VariableDefinition* rmlui_generic_row_definition() {
	static GenericRowDefinition def;
	return &def;
}
Rml::VariableDefinition* rmlui_generic_array_definition() {
	static GenericArrayDefinition def;
	return &def;
}
