#pragma once

// Utility
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <chrono>
#include <future>

// Streams
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

// Containers
#include <array>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <span>

// Engine core
#include "Eagle/Core/Core.h"
#include "Eagle/Events/ApplicationEvent.h"
#include "Eagle/Events/KeyEvent.h"
#include "Eagle/Events/MouseEvent.h"
#include "Eagle/Core/Log.h"

// 3rd party
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ankerl/unordered_dense.h>
#include <entt.hpp>
#include <magic_enum.hpp>

#ifdef EG_PLATFORM_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <Windows.h>
#endif
