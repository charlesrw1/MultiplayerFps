#pragma once
// Precompiled header for Render. Include only stable, rarely-changed, expensive-to-parse
// headers here (STL, glm, imgui, json) — anything engine/renderer code still edits often
// belongs in the individual .cpp/.h files, not here, or every touch invalidates the PCH.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "json.hpp"

#include "Debug.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
