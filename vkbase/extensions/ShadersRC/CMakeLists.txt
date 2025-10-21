cmake_minimum_required(VERSION 3.16)
project(vkbase)
set(CMAKE_CXX_STANDARD 20)


find_package(Python COMPONENTS Interpreter REQUIRED)

add_custom_command(
		OUTPUT shaderc
		COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/shaderc/utils/git-sync-deps
)

find_package(Vulkan REQUIRED)

set(SHADERC_SKIP_TESTS true)
add_subdirectory(shaderc)

add_library(ShadersRC ${WEATHER_SHARED} ShadersRC.cpp ShadersRC.hpp)
target_link_libraries(ShadersRC PRIVATE shaderc)
target_include_directories(ShadersRC PRIVATE ${Vulkan_INCLUDE_DIRS})



