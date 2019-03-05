#pragma once

#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#undef DrawState
#undef MessageBox
#undef ERROR
#undef DELETE
#undef LoadImage
#undef TRANSPARENT

#elif PLATFORM_POSIX

#define strcat_s(dst, sz, src) strncat(dst, src, sz)
#define strcpy_s(dst, sz, src) strncpy(dst, src, sz)
#define sprintf_s snprintf
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define vsprintf_s vsnprintf

#endif
