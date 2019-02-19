#include "core/timer.h"

#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include "core/os.h"
#define USE_QUERY_PERF_COUNTER 1
#endif

#if PLATFORM_LINUX || PLATFORM_OSX || PLATFORM_ANDROID
#include <sys/time.h>
#define USE_GET_TIME_OF_DAY 1
#endif

#if PLATFORM_HTML5
#include <emscripten.h>
#endif

namespace Core
{
	f64 Timer::GetAbsoluteTime()
	{
#if USE_QUERY_PERF_COUNTER
		LARGE_INTEGER time;
		LARGE_INTEGER freq;
		::QueryPerformanceCounter(&time);
		::QueryPerformanceFrequency(&freq);
		return (f64)time.QuadPart / (f64)freq.QuadPart;
#elif USE_GET_TIME_OF_DAY
		timeval time;
		::gettimeofday(&time, nullptr);
		return (f64)(time.tv_sec * 1000000 + time.tv_usec);
#elif PLATFORM_HTML5
		return emscripten_get_now();
#else
#error "Unimplemented for platform."
#endif
	}
} // namespace Core
