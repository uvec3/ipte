#include "ShadersRC.hpp"
#include <shaderc/shaderc.hpp>
#include <execution>
#include <map>





namespace vkbase::ShadersRC
{


    VkShaderModule loadSourceShader(const std::string &filename, bool save)
    {
        //std::vector<char> code=sys::load_file((filename).c_str());
        //if (code.empty())
        //    throw std::runtime_error("failed to load source shader file: "+filename);
        //
        //std::vector<char> compiledCode=compileShader(code,filename);
        //
        ////save compiled shader
        //
        //return createShaderModule(compiledCode);
        return VK_NULL_HANDLE;
    }

    VkShaderModule loadShader(const std::string &filename)
    {
        return VK_NULL_HANDLE;
    }

    shaderc_shader_kind getShaderKind(const std::string &name)
    {
        shaderc_shader_kind kind;
        std::string extension = name.substr(name.find_last_of('.') + 1);
        if (extension == "vert")
            kind = shaderc_shader_kind::shaderc_glsl_vertex_shader;
        else if (extension == "frag")
            kind = shaderc_shader_kind::shaderc_glsl_fragment_shader;
        else if (extension == "comp")
            kind = shaderc_shader_kind::shaderc_glsl_compute_shader;
        else if (extension == "tesc")
            kind = shaderc_shader_kind::shaderc_glsl_tess_control_shader;
        else if (extension == "tese")
            kind = shaderc_shader_kind::shaderc_glsl_tess_evaluation_shader;
        else if (extension == "geom")
            kind = shaderc_shader_kind::shaderc_glsl_geometry_shader;
        else if (extension == "rgen")
            kind = shaderc_shader_kind::shaderc_glsl_raygen_shader;
        else if (extension == "rint")
            kind = shaderc_shader_kind::shaderc_glsl_intersection_shader;
        else if (extension == "rahit")
            kind = shaderc_shader_kind::shaderc_glsl_anyhit_shader;
        else if (extension == "rchit")
            kind = shaderc_shader_kind::shaderc_glsl_closesthit_shader;
        else if (extension == "rmiss")
            kind = shaderc_shader_kind::shaderc_glsl_miss_shader;
        else if (extension == "rcall")
            kind = shaderc_shader_kind::shaderc_glsl_callable_shader;
        else if (extension == "mesh")
            kind = shaderc_shader_kind::shaderc_glsl_mesh_shader;
        else if (extension == "task")
            kind = shaderc_shader_kind::shaderc_glsl_task_shader;
        else if (extension == "spv")
            kind = shaderc_shader_kind::shaderc_spirv_assembly;
        else
            throw std::runtime_error("Unknown shader extension: " + extension);

        return kind;
    }

    std::vector<uint32_t> compileShader(const std::string &source, const std::string &fileName)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        //decide shader type by extension
        shaderc_shader_kind kind= getShaderKind(fileName);

        // Like -DMY_DEFINE=1
        //options.AddMacroDefinition( "MY_DEFINE", "1" );

        //disable all optimizations
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetGenerateDebugInfo();
        options.SetPreserveBindings(true);


        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.data(), source.size(), kind,
                                                                         fileName.c_str(), options);

        if ( module.GetCompilationStatus() != shaderc_compilation_status_success ) {
            throw std::runtime_error(module.GetErrorMessage());
        }

        //get compiled code as uint32_t vector
        return std::vector<uint32_t>{module.cbegin(), module.cend()};
    }

    std::vector<uint32_t>
    compileShaderHLSL(const std::string &source, const std::string &fileName, ShaderType type, const ::std::string &entryPoint, const std::map<std::string, std::string> &defines, bool glsl)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        if(glsl)
            options.SetTargetEnvironment(shaderc_target_env_opengl,shaderc_env_version_opengl_4_5);

        shaderc_shader_kind kind;
        switch (type)
        {
            case ShaderType::Vertex:
                kind = shaderc_glsl_vertex_shader;
                break;
            case ShaderType::Fragment:
                kind = shaderc_glsl_fragment_shader;
                break;
            case ShaderType::Compute:
                kind = shaderc_glsl_compute_shader;
                break;
            case ShaderType::Geometry:
                kind = shaderc_glsl_geometry_shader;
                break;
            case ShaderType::TessControl:
                kind = shaderc_glsl_tess_control_shader;
                break;
            case ShaderType::TessEvaluation:
                kind = shaderc_glsl_tess_evaluation_shader;
                break;
            default:
                throw std::runtime_error("Unknown shader type");
        }


        options.SetSourceLanguage(shaderc_source_language_hlsl);
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        //options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetPreserveBindings(true);

        for(const auto& [name,val]:defines)
        {
            options.AddMacroDefinition(name,val);
        }
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.data(), source.size(), kind,
                                                                         fileName.c_str(),entryPoint.c_str(), options);


        if ( module.GetCompilationStatus() != shaderc_compilation_status_success ) {
            throw std::runtime_error(module.GetErrorMessage());
        }

        //get compiled code as uint32_t vector
        return std::vector<uint32_t>{module.cbegin(), module.cend()};
    }

    std::string compileShaderAsm(const std::string &source, const std::string &fileName)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        //decide shader type by extension
        shaderc_shader_kind kind= getShaderKind(fileName);


        options.SetOptimizationLevel(shaderc_optimization_level_zero);


        shaderc::CompilationResult result = compiler.CompileGlslToSpvAssembly(source.data(), source.size(), kind, fileName.c_str(), options);

        if (auto status= result.GetCompilationStatus(); status != shaderc_compilation_status_success )
        {
            throw std::runtime_error(result.GetErrorMessage());
        }

        //get compiled code as uint32_t vector
        return std::string{result.begin(),result.end()};
    }

    std::string
        preprocessShaderHLSL(const std::string &source, const std::string &fileName, ShaderType type, const std::map<std::string, std::string> &defines)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        shaderc_shader_kind kind;
        switch (type)
        {
            case ShaderType::Vertex:
                kind = shaderc_glsl_vertex_shader;
            break;
            case ShaderType::Fragment:
                kind = shaderc_glsl_fragment_shader;
            break;
            case ShaderType::Compute:
                kind = shaderc_glsl_compute_shader;
            break;
            case ShaderType::Geometry:
                kind = shaderc_glsl_geometry_shader;
            break;
            case ShaderType::TessControl:
                kind = shaderc_glsl_tess_control_shader;
            break;
            case ShaderType::TessEvaluation:
                kind = shaderc_glsl_tess_evaluation_shader;
            break;
            default:
                throw std::runtime_error("Unknown shader type");
        }

        options.SetSourceLanguage(shaderc_source_language_hlsl);
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        //options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetPreserveBindings(true);

        for(const auto& [name,val]:defines)
        {
            options.AddMacroDefinition(name,val);
        }

        auto module = compiler.PreprocessGlsl(source, kind, fileName.c_str(), options);

        if ( module.GetCompilationStatus() != shaderc_compilation_status_success ) {
            throw std::runtime_error(module.GetErrorMessage());
        }

        return std::string{module.cbegin(),module.cend()};
    }

}

