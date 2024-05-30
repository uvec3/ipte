#pragma once
#include <vector>
#include <string>
#include <map>
#include <vulkan/vulkan.h>

namespace vkbase::ShadersRC
{

    enum class ShaderType
    {
        Vertex,
        Fragment,
        Compute,
        Geometry,
        TessControl,
        TessEvaluation
    };


    [[maybe_unused]] std::vector<uint32_t> compileShader(const std::string &source, const std::string &fileName);


    [[maybe_unused]] std::vector<uint32_t>
    compileShaderHLSL(const std::string &source, const std::string &fileName, ShaderType type, const ::std::string &entryPoint, const std::map<std::string, std::string> &defines, bool glsl= false);

    [[maybe_unused]] std::string compileShaderAsm(const std::string &source, const std::string &fileName);

    //read source shader file using ISysRes interface and compile it
    [[maybe_unused]] VkShaderModule loadSourceShader(const std::string &filename, bool save = false);

    //try find compiled shader file, if not found - compile it from source
    [[maybe_unused]] VkShaderModule loadShader(const std::string &filename);

}