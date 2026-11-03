#pragma once

// Force-included into every Editor .cpp (see premake5.lua)

#include "egpch.h"

#include "Eagle.h"
#include "Eagle/Core/Core.h"
#include "Eagle/Renderer/SceneRenderer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
