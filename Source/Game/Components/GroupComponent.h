#pragma once
#include "Game/EntityComponent.h"

// Minimal "empty" / pivot group node used as a parent.
//
// The scene/prefab serialization format is component-centric: NewSerialization::serialize_to_text
// emits one obj per entity keyed on its first component and skips entities with zero components, so
// a bare Entity cannot persist. "Parent to New Empty" needs a group node that survives a save/load
// round-trip, so it carries this component. It holds no data and renders nothing — it exists purely
// so the group entity serializes. (Named GroupComponent rather than "Empty" to avoid colliding with
// the Lua-defined EmptyComponent in Data/scripts/import_scripts.lua.)
class GroupComponent : public Component
{
public:
	CLASS_BODY(GroupComponent);
	GroupComponent() {}
};
