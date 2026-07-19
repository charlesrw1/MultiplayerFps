#pragma once
// Precompiled header for Core. Only stable, rarely-changed, expensive-to-parse
// headers belong here (STL, glm, imgui, json). Engine headers that get edited
// regularly (Entity.h, Model.h, Util.h, GameEnginePublic.h, ...) are deliberately
// excluded — putting them here would invalidate the whole-project PCH on every
// edit to them, turning single-file incremental builds back into full rebuilds.

#include <algorithm>
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

#include "Framework/MathLib.h"
