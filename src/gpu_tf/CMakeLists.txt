INCLUDE_DIRECTORIES(
	"${ENGINE_3RDPARTY_PATH}/The-Forge/Common_3"
)

SET(SOURCES_PUBLIC 
	"tf_backend.h"
	"tf_command_list.h"
	"tf_compile_context.h"
#	"tf_descriptor_heap_allocator.h"
#	"tf_device.h"
#	"tf_linear_heap_allocator.h"
#	"tf_linear_descriptor_allocator.h"
	"tf_resources.h"
	"tf_types.h"
	"dll.h"
)

SET(SOURCES_PRIVATE 
	"private/tf_backend.cpp"
#	"private/tf_descriptor_heap_allocator.cpp"
#	"private/tf_device.cpp"
	"private/tf_command_list.cpp"
	"private/tf_compile_context.cpp"
#	"private/tf_linear_heap_allocator.cpp"
#	"private/tf_linear_descriptor_allocator.cpp"
	"private/tf_types.cpp"
)

# Vulkan
IF(ENGINE_THEFORGE_VLK)
	ADD_ENGINE_PLUGIN(gpu_vlk ${SOURCES_PUBLIC} ${SOURCES_PRIVATE})
	TARGET_LINK_LIBRARIES(gpu_vlk core gpu)
	TARGET_COMPILE_DEFINITIONS(gpu_vlk PRIVATE -DVULKAN -DENGINE_THEFORGE_VLK)
ENDIF()

# Direct3D 12
IF(ENGINE_THEFORGE_DX12)
	ADD_ENGINE_PLUGIN(gpu_dx12 ${SOURCES_PUBLIC} ${SOURCES_PRIVATE})
	TARGET_LINK_LIBRARIES(gpu_dx12 core gpu)
	TARGET_COMPILE_DEFINITIONS(gpu_dx12 PRIVATE -DDIRECT3D12 -DENGINE_THEFORGE_DX12)
ENDIF()

# Metal
IF(ENGINE_THEFORGE_MTL)
	ADD_ENGINE_PLUGIN(gpu_mtl ${SOURCES_PUBLIC} ${SOURCES_PRIVATE})
	TARGET_LINK_LIBRARIES(gpu_mtl core gpu)
	TARGET_COMPILE_DEFINITIONS(gpu_mtl PRIVATE -DMETAL -DENGINE_THEFORGE_MTL)
ENDIF()
