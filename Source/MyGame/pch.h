#pragma once
// Precompiled header for MyGame. Include only stable, rarely-changed, expensive-to-parse
// headers here (STL, glm, imgui, json) — anything engine/gameplay code still edits often
// belongs in the individual .cpp/.h files, not here, or every touch invalidates the PCH.

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "imgui.h"
#include "json.hpp"

#include "Debug.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
