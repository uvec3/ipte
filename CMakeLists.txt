cmake_minimum_required(VERSION 3.22.1)
project(sdl-fractal)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#add shaders and templates dirs to assets
set(ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders ${CMAKE_CURRENT_SOURCE_DIR}/assets/templates ${CMAKE_CURRENT_SOURCE_DIR}/assets/other)

add_subdirectory(vkbase)
add_subdirectory(External/efsw)
add_subdirectory(External/nativefiledialog-extended)
# Disable clip examples and tests
set(CLIP_EXAMPLES OFF CACHE BOOL "Compile clip examples")
set(CLIP_TESTS OFF CACHE BOOL "Compile clip tests")
add_subdirectory(External/clip)
add_subdirectory(vkbase/external/SPIRV-Cross)

add_executable(ipte main.cpp Fractal/ShaderModel.cpp Fractal/ShaderModel.hpp Fractal/ShaderEditor.cpp Fractal/ShaderEditor.hpp
        Fractal/GLSLCompiler.cpp Fractal/SPIRVCompiler.cpp Fractal/HLSLCompiler.cpp
        Fractal/FileWatcher.hpp Fractal/FileWatcher.cpp Fractal/Fractal.cpp Fractal/BitmapGenerator.cpp vkbase/external/SPIRV-Cross/spirv_glsl.cpp vkbase/external/SPIRV-Cross/spirv_glsl.cpp)

target_link_libraries(ipte
        SDL_backend_core
        vkbase_e
        imgui
        SDL_backend_imgui
        Buffer
        imgui_textedit
        imgui_Log
        ShadersRC
        efsw
        nfd
        spirv-cross-cpp spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect
        clip
)
