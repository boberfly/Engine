#pragma once

#include "core/dll.h"
#include "core/types.h"

#include <stdarg.h>
#ifdef PLATFORM_POSIX
#include <signal.h>
#endif

namespace Core
{
	/**
	 * Log to debug channel of the platform.
	 */
	CORE_DLL void LogV(const char* Text, va_list ArgList);

	/**
	 * Log to debug channel of the platform.
	 */
	CORE_DLL void Log(const char* Text, ...);

	/**
	 * Internal assertion.
	 */
	CORE_DLL bool AssertInternal(const char* pMessage, const char* pFile, int Line, ...);

	/**
	 * @return Is debugger attached?
	 */
	CORE_DLL bool IsDebuggerAttached();

	enum class MessageBoxType
	{
		OK = 0,
		OK_CANCEL,
		YES_NO,
		YES_NO_CANCEL
	};

	enum class MessageBoxIcon
	{
		WARNING = 0,
		ERROR,
		QUESTION
	};

	enum class MessageBoxReturn
	{
		OK = 0,
		YES = 0,
		NO = 1,
		CANCEL = 2,
	};

	struct SymbolInfo
	{
		char name_[256] = {};
	};

	/**
	 * Open a message box.
	 */
	CORE_DLL MessageBoxReturn MessageBox(const char* title, const char* message,
	    MessageBoxType type = MessageBoxType::OK, MessageBoxIcon icon = MessageBoxIcon::WARNING);

	/**
	 * Get callstack.
	 * @return Number of addresses.
	 */
	CORE_DLL i32 GetCallstack(i32 skipFrames, void** addresses, i32 maxAddresses, i32* stackHash = nullptr);

	/**
	 * Get symbol info.
	 * @return Symbol info for a given @a address.
	 */
	SymbolInfo GetSymbolInfo(void* address);

	/**
	 * Set break on assertion.
	 */
	CORE_DLL void SetBreakOnAssertion(bool enableBreak);

} // namespace Core

//////////////////////////////////////////////////////////////////////////
// Macros
#if !defined(_RELEASE)
#if PLATFORM_WINDOWS
#define DBG_BREAK __debugbreak()
#elif PLATFORM_POSIX
#define DBG_BREAK raise(SIGTRAP)
#else
#define DBG_BREAK
#endif
#define DBG_ASSERT_MSG(Condition, Message, ...)                                                                        \
	if(!(Condition))                                                                                                   \
	{                                                                                                                  \
		if(Core::AssertInternal(Message, __FILE__, __LINE__, ##__VA_ARGS__))                                           \
			DBG_BREAK;                                                                                                 \
	}
#define DBG_ASSERT(Condition)                                                                                          \
	if(!(Condition))                                                                                                   \
	{                                                                                                                  \
		if(Core::AssertInternal(#Condition, __FILE__, __LINE__))                                                       \
			DBG_BREAK;                                                                                                 \
	}


#define DBG_VERIFY(Condition) DBG_ASSERT(Condition)

#define DBG_LOG(...) Core::Log(__VA_ARGS__)

#else
#define DBG_BREAK
#define DBG_ASSERT_MSG(Condition, Message, ...)
#define DBG_ASSERT(Condition)
#define DBG_VERIFY(Condition) Condition
#define DBG_LOG(...)
#endif
