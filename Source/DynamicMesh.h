#pragma once
#include "Framework/PersistentlyMappedBuffer.h"
class DynamicMesh
{
public:

private:
	PersistentlyMappedBuffer vertex_buffer;
	PersistentlyMappedBuffer index_buffer;
};

// create_dynamic_model() -> spits out a Model*, free_dynamic_model(Model*)
// create_dynamic_mesh_buffers() -> spits out DynamicMesh, then assign DynamicMesh to Model(s)
// at runtime, 