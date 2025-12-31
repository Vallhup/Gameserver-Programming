#pragma once

#define WIN_LEAN_AND_WEAN

#ifdef _DEBUG
#pragma comment(lib, "Debug\\ServerCore.lib")
#pragma comment(lib, "Debug\\lua54.lib")
#else
#pragma comment(lib, "Release\\ServerCore.lib")
#pragma comment(lib, "Release\\lua54.lib")
#endif

#include "../../SERVER/ServerCore/CorePch.h"