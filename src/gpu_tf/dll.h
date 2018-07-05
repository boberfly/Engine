#pragma once

#include "core/portability.h"

#if GPU_TF_DLL_EXPORT
#define GPU_TF_DLL EXPORT
#else
#define GPU_TF_DLL IMPORT
#endif

#if CODE_INLINE
#define GPU_TF_DLL_INLINE
#else
#define GPU_TF_DLL_INLINE GPU_DLL
#endif
