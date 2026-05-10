#pragma once
// Internal types shared across the RaytraceTest split files.
// Not part of the public API — do not include from outside Render/RT/.
#include <vector>
#include "RaytraceTest.h"

// Shared compile-time constants
inline constexpr int MAX_RAYS       = 256;
inline constexpr int ddgiIRRADTILE  = 8;
inline constexpr int ddgiDEPTHTILE  = 16;

struct VolumesAndNumProbes
{
    std::vector<DdgiVolumeGpu> volumes;
    int num_probes;
    std::vector<glm::vec4> relocate_data;
};

// Defined in RaytraceTest_BVH.cpp
VolumesAndNumProbes find_volumes();
