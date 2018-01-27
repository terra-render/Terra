cmake_minimum_required (VERSION 2.8)
project (Terra C)


set(INC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TERRA_HDRS ${INC}/Terra.h ${INC}/TerraMath.h ${INC}/TerraPresets.h ${SRC}/TerraBVH.h ${SRC}/TerraKDTree.h)
set(TERRA_SRCS ${SRC}/Terra.c ${SRC}/TerraBVH.c ${SRC}/TerraKDTree.c ${SRC}/TerraPresets.c)
add_library(Terra ${TERRA_HDRS} ${TERRA_SRCS})
target_include_directories(Terra PUBLIC ${INC})

set(TERRA_INCLUDE_DIRS ${INC} PARENT_SCOPE)
