#pragma once
#include "Framework/PropertyEd.h"

class EditorDoc;
class AssetMetadata;
class AnimationGraphEditor;
class MaterialEditorLocal;

class PropertyFactoryUtil
{
public:
	static void register_basic(FnFactory<IPropertyEditor>& factory);
	static void register_editor(EditorDoc& doc, FnFactory<IPropertyEditor>& editor);
	static void register_anim_editor(AnimationGraphEditor& ed, FnFactory<IPropertyEditor>& factory);
	static void register_mat_editor(MaterialEditorLocal& ed, FnFactory<IPropertyEditor>& factory);
};

class SharedAssetPropertyEditor : public IPropertyEditor
{
public:

	virtual std::string get_str() = 0;
	virtual void set_asset(const std::string& str) = 0;
	virtual bool is_soft_editor() const {
		return false;
	}
	virtual bool get_failed_load() const { return false; }
	virtual bool internal_update();
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { return !asset_str.empty(); }
	virtual void reset_value() {
		set_asset("");
		asset_str = "";
		//auto ptr = (IAsset**)prop->get_ptr(instance);
		//*ptr = nullptr;
	}
private:
	bool has_init = false;
	std::string asset_str;
	const AssetMetadata* metadata = nullptr;
};
class SoftAssetPropertyEditor : public SharedAssetPropertyEditor
{
public:
	SoftAssetPropertyEditor() {}
	std::string get_str() override;
	void set_asset(const std::string& str) override;
	bool is_soft_editor() const override {
		return true;
	}
};
class AssetPropertyEditor : public SharedAssetPropertyEditor
{
public:
	AssetPropertyEditor() {}
	std::string get_str() override;
	void set_asset(const std::string& str) override;
	bool get_failed_load() const override;
};


class EntityPtrAssetEditor : public IPropertyEditor
{
public:
	EntityPtrAssetEditor(EditorDoc& editor);
	~EntityPtrAssetEditor() override;
	bool internal_update() final;
	virtual int extra_row_count() { return 0; }
	virtual bool can_reset() { return false; }
	virtual void reset_value() {
	}

	EditorDoc& editor;
};

class ColorEditor : public IPropertyEditor
{
public:
	virtual bool internal_update();
	virtual int extra_row_count();
	virtual bool can_reset();
	virtual void reset_value();
private:
};

class ButtonPropertyEditor : public IPropertyEditor
{
	bool internal_update();
	bool can_reset();
};

class AnchorJointEditor : public IPropertyEditor
{
public:
	AnchorJointEditor(EditorDoc& doc) : editor(doc) {}
	~AnchorJointEditor();
	// Inherited via IPropertyEditor
	virtual bool internal_update() override;

	EditorDoc& editor;
};

class CubemapAnchorEditor : public IPropertyEditor
{
public:
	CubemapAnchorEditor(EditorDoc& doc) : editor(doc) {}
	~CubemapAnchorEditor();
	// Inherited via IPropertyEditor
	virtual bool internal_update() override;
	bool using_this = false;
	EditorDoc& editor;
};


class FindAnimGraphVariableProp : public IPropertyEditor
{
public:
	FindAnimGraphVariableProp(AnimationGraphEditor& editor) : editor(editor) {}
	virtual bool internal_update() override;
	AnimationGraphEditor& editor;
};

class FindAnimationClipPropertyEditor : public IPropertyEditor
{
public:
	FindAnimationClipPropertyEditor(AnimationGraphEditor& editor) : editor(editor) {}
	// Inherited via IPropertyEditor
	virtual bool internal_update() override;
	AnimationGraphEditor& editor;
};

class AgBoneFinder : public IPropertyEditor
{
public:
	AgBoneFinder(AnimationGraphEditor& editor);

	// Inherited via IPropertyEditor
	virtual bool internal_update() override;
	bool no_model = false;
	// copy in as std strings, could be c_strs but that opens up more room for bugs
	std::vector<std::string> bones;
	AnimationGraphEditor& editor;
};

class BlendspaceGridEd : public IPropertyEditor
{
public:
	BlendspaceGridEd(AnimationGraphEditor& editor) : editor(editor) {}
	virtual bool internal_update() override;
	AnimationGraphEditor& editor;
};

class MaterialParamPropEditor : public IPropertyEditor
{
public:
	MaterialParamPropEditor(MaterialEditorLocal& editor) : editor(editor) {}

	// Inherited via IPropertyEditor
	virtual bool internal_update() override;

	// cursed moment
	PropertyInfo pi;
	bool has_init = false;
	std::unique_ptr<IPropertyEditor> internalEditor;
	MaterialEditorLocal& editor;
};
class EntityBoneParentStringEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	~EntityBoneParentStringEditor() override;

	virtual bool internal_update() override;
	std::string str;
	bool has_init = false;
	std::vector<std::string> options;
};
class EntityTagEditor : public IPropertyEditor
{
public:
	EntityTagEditor() {}
	// Inherited via IPropertyEditor
	~EntityTagEditor() override;

	// Inherited via IPropertyEditor
	virtual bool internal_update() override;
	std::string str;
	bool has_init = false;
	std::vector<std::string> options;
};
class StateTransitionScript;
class StateTransitionPropertyEditor : public IPropertyEditor
{
public:
	bool internal_update() override;
	void draw_value_header_side(StateTransitionScript* self, bool rhs);
	void draw_value_side(StateTransitionScript* self, bool rhs);
};
class ClassTypePtrPropertyEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override;
	bool has_init = false;

	const ClassTypeInfo* type_of_base = nullptr;
};

class CodeBlockPropEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override;
	bool has_init = false;
};
