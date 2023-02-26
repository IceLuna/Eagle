#pragma once

//Utility
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <chrono>

//Streams
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

//Containers
#include <array>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

//Engine core
#include "Eagle/Core/Core.h"
#include "Eagle/Core/Log.h"

#ifdef EG_PLATFORM_WINDOWS
	#include <Windows.h>
#endif
