cmake_minimum_required(VERSION 3.22.1)
project(IPTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#Development mode
set(DEV OFF CACHE BOOL "Dev build")

if(DEV)#Build shared libs when in DEV mode
    set(WEATHER_SHARED SHARED)
else ()
    set(WEATHER_SHARED "")
endif ()

#add shaders and templates dirs to assets
set(ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders ${CMAKE_CURRENT_SOURCE_DIR}/assets/templates ${CMAKE_CURRENT_SOURCE_DIR}/assets/other)

add_subdirectory(vkbase)
add_subdirectory(vkbase/external/SPIRV-Cross)

set(BUILD_SHARED_LIBS ${WEATHER_SHARED})
    add_subdirectory(External/efsw)
    add_subdirectory(External/nativefiledialog-extended)
    # Disable clip examples and tests
    set(CLIP_EXAMPLES OFF CACHE BOOL "Compile clip examples")
    set(CLIP_TESTS OFF CACHE BOOL "Compile clip tests")
    add_subdirectory(External/clip)
set(BUILD_SHARED_LIBS OFF)

add_executable(ipte main.cpp Fractal/ShaderModel.cpp Fractal/ShaderModel.hpp Fractal/ShaderEditor.cpp Fractal/ShaderEditor.hpp
        Fractal/GLSLCompiler.cpp Fractal/SPIRVCompiler.cpp Fractal/HLSLCompiler.cpp
        Fractal/FileWatcher.hpp Fractal/FileWatcher.cpp Fractal/Fractal.cpp Fractal/BitmapGenerator.cpp)

# create intermidiete libs to improve building time
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
        ShadersSlang
        efsw
        nfd
        clip
        spirv_lib
        vkbase_lib
)

#Copy ddls for shared build
if(WEATHER_SHARED)
    add_custom_command(
            TARGET ipte POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying runtime DLLs for ipte"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ipte> $<TARGET_FILE_DIR:ipte>
            COMMAND_EXPAND_LISTS
    )
endif()