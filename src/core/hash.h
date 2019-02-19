#pragma once

#include "core/dll.h"
#include "core/types.h"

namespace Core
{
	/**
	 * MD5.
	 */
	struct CORE_DLL HashMD5Digest
	{
		union CORE_DLL
		{
			u8 data8_[16] = {0};
			u32 data32_[4];
			u64 data64_[2];
		};
	};

	CORE_DLL HashMD5Digest HashMD5(const void* data, size_t size);

	/**
	 * SHA-1.
	 */
	struct CORE_DLL HashSHA1Digest
	{
		union CORE_DLL
		{
			u8 data8_[20] = {0};
			u32 data32_[5];
		};
	};

	CORE_DLL HashSHA1Digest HashSHA1(const void* data, size_t size);

	/**
	 * 32-bit hashing algorithms.
	 */
	CORE_DLL u32 HashCRC32(u32 Input, const void* pInData, size_t Size);
	CORE_DLL u32 HashSDBM(u32 Input, const void* pInData, size_t Size);

	/**
	 * 64-bit hashing algorithms.
	 */
	CORE_DLL u64 HashFNV1a(u64 input, const void* pInData, size_t size);

	/**
	 * Templated hash function to ensure users define their own.
	 */
#ifdef PLATFORM_WINDOWS
	template<typename TYPE>
	inline u64 Hash(u64 Input, const TYPE& Data)
	{
		static_assert(false, "Hash function not defined for type. Did you define it in the Core namespace?");
		return 0;
	}
#endif

	/**
	 * Specialised hash functions.
	 */
	CORE_DLL u64 Hash(u64 Input, const char* Data);
	inline u64 Hash(u64 Input, u8 Data) { return HashFNV1a(Input, &Data, 1); }
	inline u64 Hash(u64 Input, u16 Data) { return HashFNV1a(Input, &Data, 2); }
	inline u64 Hash(u64 Input, u32 Data) { return HashFNV1a(Input, &Data, 4); }
	inline u64 Hash(u64 Input, u64 Data) { return HashFNV1a(Input, &Data, sizeof(Data)); }
	inline u64 Hash(u64 Input, i8 Data) { return HashFNV1a(Input, &Data, 1); }
	inline u64 Hash(u64 Input, i16 Data) { return HashFNV1a(Input, &Data, 2); }
	inline u64 Hash(u64 Input, i32 Data) { return HashFNV1a(Input, &Data, 4); }
	inline u64 Hash(u64 Input, i64 Data) { return HashFNV1a(Input, &Data, sizeof(Data)); }

	/**
	 * Default hasher.
	 */
	template<typename TYPE>
	class Hasher
	{
	public:
		u64 operator()(u64 input, const TYPE& data) const { return Hash(input, data); }
	};

} // namespace Core
