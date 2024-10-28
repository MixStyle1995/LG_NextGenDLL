#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <Urlmon.h>
#include <Storm.h>
#include <atlenc.h>
#include <array>
#include <sstream>

#include "XorStr.hpp"

#pragma comment(lib, "Storm.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "winhttp.lib")

using namespace std;