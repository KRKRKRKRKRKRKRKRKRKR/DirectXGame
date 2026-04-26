#pragma once
//window
#include <windows.h>
#include <strsafe.h>

// DirectX
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12sdklayers.h>
#include <d3d12shader.h>
#include <dxgidebug.h>
#include <dxgi1_3.h>
#include <synchapi.h>
#include <DirectXMath.h>
#include <dbghelp.h>

// STL
#include <string>
#include <cstdint>
#include <cassert>
#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>

// Lib
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "d3dcompiler.lib")

//Math
#include "Math/MathTypes.h"