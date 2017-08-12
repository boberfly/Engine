SET(SOURCES_PUBLIC 
	"vlk_backend.h"
	"vlk_command_list.h"
	"vlk_compile_context.h"
	"vlk_descriptor_heap_allocator.h"
	"vlk_device.h"
	"vlk_linear_heap_allocator.h"
	"vlk_resources.h"
	"vlk_types.h"
	"dll.h"
)

SET(SOURCES_PRIVATE 
	"private/vlk_backend.cpp"
	"private/vlk_descriptor_heap_allocator.cpp"
	"private/vlk_device.cpp"
	"private/vlk_command_list.cpp"
	"private/vlk_compile_context.cpp"
	"private/vlk_linear_heap_allocator.cpp"
	"private/vlk_types.cpp"
)

SET(SOURCES_TESTS
	"tests/test_entry.cpp"
)

# Add gpu_vlk.
ADD_LIBRARY(gpu_vlk SHARED ${SOURCES_PUBLIC} ${SOURCES_PRIVATE})
SET_TARGET_PROPERTIES(gpu_vlk PROPERTIES FOLDER Libraries)
TARGET_LINK_LIBRARIES(gpu_vlk core gpu)
TARGET_COMPILE_DEFINITIONS(gpu_vlk PRIVATE GPU_VLK_EXPORT=1)

SOURCE_GROUP("Public" FILES ${SOURCES_PUBLIC})
SOURCE_GROUP("Private" FILES ${SOURCES_PRIVATE})
