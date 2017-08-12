#pragma once

#include "core/portability.h"

#if GPU_VLK_DLL_EXPORT
#define GPU_VLK_DLL EXPORT
#else
#define GPU_VLK_DLL IMPORT
#endif

#if CODE_INLINE
#define GPU_VLK_DLL_INLINE
#else
#define GPU_VLK_DLL_INLINE GPU_DLL
#endif
