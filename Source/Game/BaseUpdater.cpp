#include "BaseUpdater.h"
#include "Level.h"
#include "GameEnginePublic.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/ReflectionMacros.h"

CLASS_IMPL(BaseUpdater);

void BaseUpdater::set_ticking(bool shouldTick)
{
	auto level = eng->get_level();
	if (level&&tickEnabled != shouldTick&&init_state==initialization_state::CALLED_START) {
		if (shouldTick)
			level->add_to_update_list(this);
		else
			level->remove_from_update_list(this);
	}
	tickEnabled = shouldTick;
}
void BaseUpdater::init_updater()
{
	auto level = eng->get_level();
	ASSERT(level);
	ASSERT(init_state == initialization_state::CALLED_PRE_START);
	if (level && tickEnabled)
		level->add_to_update_list(this);
}
void BaseUpdater::shutdown_updater()
{
	auto level = eng->get_level();
	ASSERT(level);
	if (level && tickEnabled)
		level->remove_from_update_list(this);
}
void BaseUpdater::activate_internal_step1()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		pre_start();
	}
	init_state = initialization_state::CALLED_PRE_START;
}
void BaseUpdater::activate_internal_step2()
{
	ASSERT(init_state == initialization_state::CALLED_PRE_START);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		start();
		init_updater();
	}
	init_state = initialization_state::CALLED_START;
}

void BaseUpdater::deactivate_internal()
{
	ASSERT(init_state == initialization_state::CALLED_START);
	if (!eng->is_editor_level() || get_call_init_in_editor()) {
		end();
		shutdown_updater();
	}
	init_state = initialization_state::HAS_ID;
}
const PropertyInfoList* BaseUpdater::get_props()
{
	START_PROPS(BaseUpdater)
		REG_FUNCTION_EXPLICIT_NAME(get_type_for_script,"get_type"),
		REG_FUNCTION_EXPLICIT_NAME(is_type_for_script, "is_type"),
	END_PROPS(BaseUpdater)
}