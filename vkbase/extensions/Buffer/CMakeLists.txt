cmake_minimum_required(VERSION 3.20)
project(vkbase)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_library(Buffer Buffer.cpp Buffer.hpp)
target_include_directories(Buffer PRIVATE ${Vulkan_INCLUDE_DIRS})