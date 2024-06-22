#pragma once

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM 

#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable : 4995)
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
#pragma comment(lib, "strmbasd.lib")
#else
#pragma comment(lib, "strmbase.lib")
#endif

#include <windows.h>
#include <tchar.h>
#include <streams.h>
#include <initguid.h>

#include "MyAsyncSrcFilter.h"
