cmake_minimum_required (VERSION 2.8)
#set(CMAKE_C_COMPILER clang)
project (Satellite CXX)

file(GLOB_RECURSE SATELLITE_HDRS include/App.hpp include/Cloto.h include/Console.hpp include/Events.hpp include/Graphics.hpp include/Logging.hpp include/Renderer.hpp include/Scene.hpp include/Visualization.hpp)
file(GLOB_RECURSE SATELLITE_SRCS src/App.cpp src/Console.cpp src/Graphics.cpp src/Logging.cpp src/Renderer.cpp src/Satellite.cpp src/Scene.cpp src/Visualization.cpp)
file(GLOB_RECURSE EXT_SRCS dependencies/imgui/*.hpp dependencies/imgui/*.cpp dependencies/ dependencies/Terra/include/*.h dependencies/Terra/src/*.cpp)

add_executable(Satellite ${SATELLITE_HDRS} ${SATELLITE_SRCS} ${EXT_SRCS})

target_include_directories(Satellite PUBLIC 
include/ 
dependencies 
dependencies/glfw/include 
dependencies/Terra/include
dependencies/Apollo
)

target_compile_options(Satellite PUBLIC -pedantic -Wall -fpermissive)
target_compile_definitions(Satellite PUBLIC OPENGL_DEBUG)

set_target_properties(Satellite PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

target_link_libraries(Satellite PUBLIC Opengl32 
${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glew/lib/Release/x64/glew32s.lib 
${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glfw/lib-vc2015/glfw3.lib)
target_compile_definitions(Satellite PUBLIC GLEW_STATIC DEV)