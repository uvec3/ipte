cmake_minimum_required(VERSION 3.28 )
project(IPTE)
set(CMAKE_CXX_STANDARD 23)

#add shaders and templates dirs to assets
set(ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders  ${CMAKE_CURRENT_SOURCE_DIR}/assets/include ${CMAKE_CURRENT_SOURCE_DIR}/assets/templates ${CMAKE_CURRENT_SOURCE_DIR}/assets/other)

include(FetchContent)
FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
        URL_MD5 893b5203b862eb9bbd08553e24ff146a
        DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(BOOST_INCLUDE_LIBRARIES regex multi_index lambda)
FetchContent_MakeAvailable(Boost)

add_subdirectory(vkbase EXCLUDE_FROM_ALL)
add_subdirectory(vkbase/external/SPIRV-Cross EXCLUDE_FROM_ALL)

add_subdirectory(External/efsw EXCLUDE_FROM_ALL)
add_subdirectory(External/nativefiledialog-extended EXCLUDE_FROM_ALL)
# Disable clip examples and tests
set(CLIP_EXAMPLES OFF CACHE BOOL "Compile clip examples")
set(CLIP_TESTS OFF CACHE BOOL "Compile clip tests")
add_subdirectory(External/clip EXCLUDE_FROM_ALL)

add_executable(ipte main.cpp Fractal/ShaderModel.cpp Fractal/ShaderModel.hpp Fractal/ShaderEditor.cpp Fractal/ShaderEditor.hpp
         Fractal/HLSLCompiler.cpp
        Fractal/FileWatcher.hpp Fractal/FileWatcher.cpp Fractal/Fractal.cpp Fractal/BitmapGenerator.cpp
        Fractal/Project.cpp Fractal/Project.hpp)

# create intermediate libs to improve building time
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp" "")
add_library(spirv_lib ${WEATHER_SHARED} ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp)
add_library(vkbase_lib ${WEATHER_SHARED} ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp)

target_link_libraries(spirv_lib spirv-cross-cpp spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect)
target_link_libraries(vkbase_lib
        vkbase_e
        imgui
        SDL_backend_core
        SDL_backend_imgui
        Buffer
        imgui_textedit
        imgui_Log)

#Linking app
target_link_libraries(ipte
        ShadersRC
        vkbase::assets
        ShadersSlang
        efsw
        nfd
        clip
        spirv_lib
        vkbase_lib
        Boost::regex
        Boost::multi_index
        Boost::lambda
)
