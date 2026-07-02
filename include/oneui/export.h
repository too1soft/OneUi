#pragma once

#if defined(ONEUI_STATIC)
#define ONEUI_API
#elif defined(_WIN32)
#if defined(ONEUI_BUILDING_DLL)
#define ONEUI_API __declspec(dllexport)
#else
#define ONEUI_API __declspec(dllimport)
#endif
#else
#define ONEUI_API __attribute__((visibility("default")))
#endif
