#include <fstream>
#include <iostream>
#include "ShaderModel.hpp"
#include "HLSLCompiler.h"
#include <spirv_reflect.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <variant>
#include <shaderc/shaderc.hpp>

#include "../vkbase/extensions/Assets/assets.h"


class NonConvertible{};

using ParameterToConvert = std::variant<NonConvertible *, int *, glm::ivec2 *, glm::ivec3 *, glm::ivec4 *,
        float *, glm::vec2 *, glm::vec3 *, glm::vec4 *,
        double *, glm::dvec2 *, glm::dvec3 *, glm::dvec4 *,
        uint32_t *, glm::uvec2 *, glm::uvec3 *, glm::uvec4 *>;

ParameterToConvert getConvertable(UniformParameter &p)
{
    if(p.type == "int")
        return reinterpret_cast<int *>(p.data.data());
    if(p.type == "ivec2")
        return reinterpret_cast<glm::ivec2 *>(p.data.data());
    if(p.type == "ivec3")
        return reinterpret_cast<glm::ivec3 *>(p.data.data());
    if(p.type == "ivec4")
        return reinterpret_cast<glm::ivec4 *>(p.data.data());
    if(p.type == "float")
        return reinterpret_cast<float *>(p.data.data());
    if(p.type == "vec2")
        return reinterpret_cast<glm::vec2 *>(p.data.data());
    if(p.type == "vec3")
        return reinterpret_cast<glm::vec3 *>(p.data.data());
    if(p.type == "vec4")
        return reinterpret_cast<glm::vec4 *>(p.data.data());
    if(p.type == "double")
        return reinterpret_cast<double *>(p.data.data());
    if(p.type == "dvec2")
        return reinterpret_cast<glm::dvec2 *>(p.data.data());
    if(p.type == "dvec3")
        return reinterpret_cast<glm::dvec3 *>(p.data.data());
    if(p.type == "dvec4")
        return reinterpret_cast<glm::dvec4 *>(p.data.data());
    if(p.type == "uint")
        return reinterpret_cast<uint32_t *>(p.data.data());
    if(p.type == "uvec2")
        return reinterpret_cast<glm::uvec2 *>(p.data.data());
    if(p.type == "uvec3")
        return reinterpret_cast<glm::uvec3 *>(p.data.data());
    if(p.type == "uvec4")
        return reinterpret_cast<glm::uvec4 *>(p.data.data());
    return reinterpret_cast<NonConvertible *>(0);
}

bool convert(ParameterToConvert &from, ParameterToConvert &to)
{
    return std::visit([&to](auto &fromPtr) -> bool
                      {
                          return std::visit([&fromPtr](auto &toPtr) -> bool
                                            {
                                                if constexpr(std::is_convertible<decltype(*fromPtr), typename std::remove_reference<decltype(*toPtr)>::type>())
                                                {
                                                    *toPtr = static_cast<std::remove_reference<decltype(*toPtr)>::type>(*fromPtr);
                                                    return true;
                                                }
                                                return false;

                                            }, to);

                      }, from);
}

UniformParameters::UniformParameters(UniformParameters oldParameters, const std::string &reflection)
{
    try
    {
        nlohmann::json j = nlohmann::json::parse(reflection.c_str());

        std::string buffType = j["ssbos"][0]["type"];
        std::string structType = j["types"][buffType]["members"][0]["type"];
        nlohmann::json parametersJson = j["types"][structType]["members"];
        size = j["types"][buffType]["members"][0]["array_stride"];
        //std::cout << parametersJson;

        UniformParameter *previous = nullptr;
        for(auto &pjson: parametersJson)
        {
            auto &parameter = activeParameters[pjson["name"]];
            parameter.name = pjson["name"];
            parameter.type = pjson["type"];
            parameter.offset = pjson["offset"];
            if(previous)
                previous->data.resize(parameter.offset - previous->offset, 0);
            previous = &parameter;
        }
        if(previous)
            previous->data.resize(size - previous->offset, 0);
    }
    catch(const std::exception &e)
    {
        std::cout << "No parameters found!\n";
    }

    //copy or remove previous
    for(auto &[name, p]: oldParameters.activeParameters)
    {
        if(activeParameters.contains(name))//if exists
        {
            if(p.type == activeParameters[name].type)//copy if type matches
            {
                memcpy(activeParameters[name].data.data(), p.data.data(), std::min(p.size(), activeParameters[name].size()));
            } else//try to convert for different types
            {
                auto from = getConvertable(p);
                auto to = getConvertable(activeParameters[name]);
                convert(from, to);
            }
            activeParameters[name].metadata = p.metadata;
            activeParameters[name].isDynamic = p.isDynamic;
        } else//move to removed
        {
            removedParameters[name] = p;
        }
    }

    //restore previously removed
    for(auto &[name, p]: oldParameters.removedParameters)
    {
        if(activeParameters.contains(name))//try to restore
        {
            if(p.type == activeParameters[name].type)
            {
                memcpy(activeParameters[name].data.data(), p.data.data(), std::min(p.size(), activeParameters[name].size()));
            } else//or try to convert
            {
                auto from = getConvertable(p);
                auto to = getConvertable(activeParameters[name]);
                convert(from, to);
            }

            activeParameters[name].metadata = p.metadata;
            activeParameters[name].isDynamic = p.isDynamic;
        } else//copy others removed
        {
            removedParameters[name] = p;
        }
    }
}

std::vector<char> UniformParameters::buildBuffer()
{
    std::vector<char> buff(size, 0);
    for(auto &[k, p]: activeParameters)
    {
        memcpy((void *) (buff.data() + p.offset), p.data.data(), p.size());
    }
    return buff;
}

void UniformParameters::readFromBuffer(void *buffer)
{
    for(auto &[k, p]: activeParameters)
    {
        memcpy(p.data.data(), (reinterpret_cast<char *>(buffer) + p.offset), p.size());
    }
}

nlohmann::json UniformParameters::serialize()
{
    nlohmann::json result;
    result["size"] = size;


    for(auto &[name, p]: activeParameters)
    {
        result["members"][name]["name"] = p.name;
        result["members"][name]["type"] = p.type;
        result["members"][name]["offset"] = p.offset;
        result["members"][name]["data"] = p.data;
        result["members"][name]["metadata"] = p.metadata;
        result["members"][name]["isStatic"] = p.isDynamic;
    }

    // std::cout<<result;

    return result;
}

void UniformParameters::deserialize(nlohmann::json j)
{
    activeParameters.clear();
    removedParameters.clear();
    size = j["size"];

    for(auto &[name, p]: j["members"].items())
    {
        activeParameters[name].name = p["name"];
        activeParameters[name].type = p["type"];
        activeParameters[name].offset = p["offset"];
        activeParameters[name].data = p["data"];
        activeParameters[name].metadata = p["metadata"];
        if(p.contains("isStatic"))
            activeParameters[name].isDynamic = p["isStatic"];
    }
}

std::string UniformParameters::initStructureString(const std::string &varName)
{
    std::stringstream ss;
    //setup stream maximal precision for float and double
    ss << std::setprecision(30);

    for(auto &[name, p]: activeParameters)
    {
        if(p.isDynamic)
        {
            ss<<varName<<"."<<name<<"=_arg_"<<name<<";\n";
        } else
        {
            if(p.type == "int")
                ss << varName << "." << name << "=" << p.get<int>() << ";\n";
            else if(p.type == "float")
                ss << varName << "." << name << "=" << p.get<float>() << ";\n";
            else if(p.type == "uint")
                ss << varName << "." << name << "=" << p.get<uint32_t>() << ";\n";
            else if(p.type == "double")
                ss << varName << "." << name << "=" << p.get<double>() << ";\n";
            else if(p.type.find("vec") != std::string::npos)
            {
                std::string s;
                s += p.type.back();
                int count = std::stoi(s);
                char type = p.type[0];

                for(int i = 0; i < count; ++i)
                {
                    ss << varName << "." << name << "[" << i << "]=";
                    switch(type)
                    {
                        case 'v':
                            ss << p.get<glm::vec4>()[i];
                            break;
                        case 'i':
                            ss << p.get<glm::ivec4>()[i];
                            break;
                        case 'u':
                            ss << p.get<glm::uvec4>()[i];
                            break;
                        case 'd':
                            ss << p.get<glm::dvec4>()[i];
                            break;
                        case 'b':
                            ss << p.get<glm::bvec4>()[i];
                            break;
                        default:
                            break;
                    }
                    ss << ";\n";
                }
            } else if(p.type.find("mat") != std::string::npos)
            {
                std::string sm;
                sm += p.type[p.type.size() - 2];

                std::string sn;
                sn += p.type.back();

                int n = std::stoi(sn);
                int m;
                try
                {
                    m = std::stoi(sm);
                }
                catch(const std::exception &e)
                {
                    m = n;
                }

                char type = p.type[0];
                for(int i = 0; i < m; ++i)
                {
                    for(int j = 0; j < n; ++j)
                    {
                        ss << varName << "." << name << "[" << i << "][" << j << "]=";

                        switch(type)
                        {
                            case 'm':
                                ss << reinterpret_cast<float *>(p.data.data())[i * n + j];
                                break;
                            case 'i':
                                ss << reinterpret_cast<int *>(p.data.data())[i * n + j];
                                break;
                            case 'u':
                                ss << reinterpret_cast<uint32_t *>(p.data.data())[i * n + j];
                                break;
                            case 'd':
                                ss << reinterpret_cast<double *>(p.data.data())[i * n + j];
                                break;
                            default:
                                break;
                        }
                        ss << ";\n";
                    }
                }
            }
        }

    }

    return ss.str();
}

std::string UniformParameters::dynamicParametersString()
{
    std::stringstream ss;
    std::string sep=" ";
    for(auto &[name, p]: activeParameters)
    {
        if(p.isDynamic)
        {
            ss <<sep << glslToHlsl(p.type) << " _arg_" << name;
            sep= ", ";
        }
    }

    return ss.str();
}


ShaderModel::ShaderModel(std::string name) : name(std::move(name))
{
    currentCompiler = &hlsl_compiler;
    keysBuffer.create(sizeof(keys), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    createDescriptorSetLayout2();
    recreateSurfaceDependentObjects();
    createComputeFence();
    createComputeCommandBuffer();


    updateCommandBuffers = true;
    OnDataUpdateReceiver::disable();
}


void ShaderModel::setSource(const std::string &src)
{
    m_source=src;
    recompileFlag=true;
}

const std::string& ShaderModel::getSource() const
{
    return m_source;
}

void ShaderModel::recreateSurfaceDependentObjects()
{
    for (auto& buff:uniformBuffers)
        buff.destroy();

    vkDestroyDescriptorPool(vkbase::device,descriptorPoolStatic,nullptr);
    vkDestroyDescriptorPool(vkbase::device,descriptorPoolDynamic,nullptr);
    vkDestroyCommandPool(vkbase::device,commandPool,nullptr);


    createDescriptorPools();
    createDescriptorSets2();
    createUniformBuffer();

    createCommandPool();
    createRenderBuffers();


    //sync parameters
    for(int i = 0; i < vkbase::imageCount; i++)
    {
        updateUniform(i);
    }

}

std::map<std::string, std::string> glslToHlslType
        {
                {"vec2",      "float2"},
                {"vec3",      "float3"},
                {"vec4",      "float4"},
                {"mat2",      "float2x2"},
                {"mat3",      "float3x3"},
                {"mat4",      "float4x4"},
                {"mat2x3",    "float2x3"},
                {"mat2x4",    "float2x4"},
                {"mat3x2",    "float3x2"},
                {"mat3x4",    "float3x4"},
                {"mat4x2",    "float4x2"},
                {"mat4x3",    "float4x3"},
                {"ivec2",     "int2"},
                {"ivec3",     "int3"},
                {"ivec4",     "int4"},
                {"uvec2",     "uint2"},
                {"uvec3",     "uint3"},
                {"uvec4",     "uint4"},
                {"sampler2D", "Texture2D"},
                {"sampler3D", "Texture3D"},

        };

std::string glslToHlsl(const std::string &glslT)
{
    if(glslToHlslType.contains(glslT))
        return glslToHlslType[glslT];
    return glslT;
}

std::string exportFunction(const std::vector<uint32_t> &spirv, const std::string &exportFuncName = "generatedFunc", bool onlyBody = false, bool outGLSL = true)
{
    spirv_cross::CompilerReflection compilerReflection(spirv);
    nlohmann::json reflection = nlohmann::json::parse(compilerReflection.compile());
    //std::cout<<reflection;

    std::string shaderCode;
    if(outGLSL)
    {
        spirv_cross::CompilerGLSL compilerGLSL(spirv);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 200;
        compilerGLSL.set_common_options(options);
        shaderCode = compilerGLSL.compile();
    } else
    {
        spirv_cross::CompilerHLSL compilerHLSL(spirv);
        spirv_cross::CompilerHLSL::Options options;
        options.shader_model = 20;
        compilerHLSL.set_hlsl_options(options);
        shaderCode = compilerHLSL.compile();
    }

    std::cout << shaderCode;

    //find entry point
    std::string entryPointPrefix;
    if(outGLSL)
        entryPointPrefix = "void main()\n{";
    else
        entryPointPrefix = "void frag_main()\n{";
    auto beginPos = shaderCode.find(entryPointPrefix);
    if(beginPos == std::string::npos)
    {
        throw std::runtime_error("Entry point not found!");
    }

    auto endPos = beginPos + entryPointPrefix.size();
    int bracketCount = 1;
    for(; endPos < shaderCode.size(); endPos++)
    {
        if(shaderCode[endPos] == '{')
            bracketCount++;
        else if(shaderCode[endPos] == '}')
            bracketCount--;
        if(bracketCount == 0)
            break;
    }
    if(endPos == shaderCode.size())
    {
        throw std::runtime_error("Entry point not found!");
    }

    beginPos += entryPointPrefix.size();


    //get constants
    std::string constPrefix;
    if(outGLSL)
        constPrefix = "const";
    else
        constPrefix = "static";
    std::string constants;
    size_t i = 0;
    while((i = shaderCode.find(constPrefix, i)) != std::string::npos)
    {
        auto end = shaderCode.find(';', i);

        //check if not constant
        if(shaderCode.substr(i + constPrefix.size(), 6) != " const")
        {
            auto lastSpace = shaderCode.rfind(' ', end);
            std::string varName = shaderCode.substr(lastSpace + 1, end - lastSpace - 1);

            bool skip = false;
            for(auto p: reflection["inputs"])//skip if input parameter
            {
                if(p["name"].get<std::string>() == varName)
                {
                    skip = true;
                    break;
                }
            }
            if(skip)
            {
                i = end;
                continue;
            }
            std::cout <<"var_name: "<< varName << std::endl;
        }

        std::string globalVar = shaderCode.substr(i, end - i + 1);
        if(globalVar!="static float4 _entryPointOutput;")
            constants += "\n    " + globalVar;
        i = end;
    }


    std::string funcBody = shaderCode.substr(beginPos, endPos - beginPos);
    std::string entryPointOutput = "entryPointParam_output = ";
    funcBody.replace(funcBody.find(entryPointOutput), entryPointOutput.size(), "return ");

    std::string type = reflection["outputs"][0]["type"].get<std::string>();
    if(!outGLSL)
        type = glslToHlsl(type);
    std::string funcHeader = type + " " + exportFuncName + "(";

    for(auto &input: reflection["inputs"])
    {
        type = input["type"].get<std::string>();
        if(!outGLSL)
            type = glslToHlsl(type);
        funcHeader += type + " " + input["name"].get<std::string>() + ",";
    }
    if(!reflection["inputs"].empty())
        funcHeader.pop_back();


    if(onlyBody)
    {
        funcHeader="//"+funcHeader+")\n";
    }
    else
    {
        funcHeader += ")\n{";
    }


    std::string func= funcHeader + constants + funcBody;
    if(!onlyBody)
        func += "}";

    //    //translate to HLSL
    //    std::string glslTemplate ="#version 450\n"+glslFunc+"\nvoid main(){}";
    //    auto spirvFromGlsl=vkbase::ShadersRC::compileShader(glslTemplate, "temp.frag");
    //    //convert to HLSL
    //    spirv_cross::CompilerHLSL compilerHLSL(spirvFromGlsl);
    //    auto codeHLSL = compilerHLSL.compile();
    //    std::cout << codeHLSL;

    // std::map<std::string , std::string> defines;
    // if(!outGLSL)
    //     defines["mod(a,b)"]="(a-floor(a/b)*b)";

    return func;
}


void ShaderModel::recompile()
{
    if (!recompileFlag)
        return;
    recompileFlag=false;

    using namespace  std::chrono;
    auto start_time = high_resolution_clock::now();
    slang_project.updateDependencies(m_source);

    status = COMPILING;
    auto paths =  slang_project.getRoot().empty()? std::vector<std::string>{} : std::vector{slang_project.getRoot().string()};

    compilationTaskManager.runTask([
        name=name,
        src=m_source,
        paths=paths,
        currentCompiler=currentCompiler,
        setLayout2=descriptorSet2Layout,
        root_path=slang_project.getRoot().generic_string()](std::stop_token stop_token)
    {
        std::cout << "+Compilation started:" << name << "\n";

        vkbase::ShadersRC::CompilationResult frag_result{};
        vkbase::ShadersRC::CompilationResult compute_result{};
        std::thread t([&]()        {
            compute_result = currentCompiler->compileCompute(src, name, root_path, paths);
        });

        frag_result = currentCompiler->compile(src, name, root_path, paths);

        t.join();

        if (stop_token.stop_requested())
        {
            std::cout<< "-Compilation cancelled:" << name << " \n";
            return std::tuple{
                frag_result, compute_result, std::string(), PipelineData{}
            };
        }

        std::string reflection;
        if (frag_result.success && compute_result.success)
        {
           spirv_cross::CompilerReflection compilerReflection(frag_result.spirv);
           reflection = compilerReflection.compile();
          // std::cout<<reflection;
        }



        // //update pipelines
        PipelineData pipeline_data{};
        if (frag_result.success && compute_result.success)
        {

            try
            {
                pipeline_data=PipelineData(frag_result.spirv,compute_result.spirv,setLayout2->get());
            }
            catch (std::exception &e)
            {
                std::cout<<"Pipeline creation failed: "<<e.what()<<"\n";
                frag_result.success=false;
                compute_result.success=false;
            }
        }
       std::cout << "-Compilation finished:" << name << " \n";
       return std::tuple{
           frag_result, compute_result, reflection, std::move(pipeline_data)
       };
    },
    [=,this](auto&& result)
    {
       auto& [frag_result,compute_result,reflection,pipeline_data] = result;
       compilation_time = (high_resolution_clock::now() - start_time).count() /
           1000000000.0;
       newUniformParametersReflection = reflection;
       fragment_shader = std::move(frag_result);
       compute_shader = std::move(compute_result);
       isChanged = true;
       if (frag_result.success && compute_result.success && pipeline_data.isValid())
       {
           vkQueueWaitIdle(vkbase::graphicsQueue);
           pipeline= std::move(pipeline_data);
           syncCompilation();
           std::cout << "=Binary code updated:" << name << "\n";
           status = COMPILED;
       }
       else
       {
           status = ERROR;
       }

    });
}

void ShaderModel::syncCompilation()
{
    if(!fragment_shader.spirv.empty())
    {
        //update parameters
        uniformParameters = UniformParameters(uniformParameters, newUniformParametersReflection);

        vkDeviceWaitIdle(vkbase::device);

        createDescriptorSets();

        //update buffers and descriptors
        if(uniformParameters.size > uniformBuffers[0].info.size)
        {
            for(int i = 0; i < vkbase::imageCount; ++i)
            {
                if(uniformBuffers[i].info.size < uniformParameters.size)
                {
                    uniformBuffers[i].create(uniformParameters.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                    parametersIntermediateBuffer.create(uniformParameters.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                    updateDescriptorSet(i);
                }
            }
        }

        updateComputeDescriptorSet();

        updateCommandBuffers = true;
    }
}

nlohmann::json ShaderModel::toJson()
{
    nlohmann::json json;
    json["name"] = name;
    json["languageName"] = currentCompiler->languageName;
    json["source"] = m_source;
    json["parameters"] = uniformParameters.serialize();
    return json;
}

void ShaderModel::loadFromJson(const nlohmann::json &json)
{
    name = json.at("name");
    if (json.contains(language_name))
        language_name=json.at("languageName");
    else
        language_name="HLSL";
    if (json.contains("source"))
        m_source=json.at("source");
    else
        m_source=json.at("compilers").at(0).at("source");

    if(language_name == "HLSL")
        currentCompiler=&hlsl_compiler;
    else
        throw std::runtime_error("Unsupported shader language: " + language_name);

    if(json.contains("parameters"))
        uniformParameters.deserialize(json["parameters"]);
    recompileFlag=true;
}

void ShaderModel::setViewArea(glm::vec4 newArea)
{
    if(newArea != viewArea)
    {
        viewArea = newArea;
        updateCommandBuffers = true;
    }
}

AbstractShaderCompiler &ShaderModel::getCurrentCompiler()
{
    return *currentCompiler;
}

const glm::vec4 &ShaderModel::getViewArea()
{
    return viewArea;
}

void ShaderModel::setActive(bool active)
{
    if(active && writeMainBufferCallbackId == -1)
    {
        writeMainBufferCallbackId = vkbase::addWriteMainBufferCallback(WRAP_MEMBER_FUNC(writeCommandBuffer));
        prepareCallbackId = vkbase::addDrawPrepareCallback(WRAP_MEMBER_FUNC(prepare),10);
        vkbase::OnDataUpdateReceiver::enable();
    } else if(!active && writeMainBufferCallbackId != -1)
    {
        vkbase::removeWriteMainBufferCallback(writeMainBufferCallbackId);
        vkbase::removeDrawPrepareCallback(prepareCallbackId);
        vkbase::OnDataUpdateReceiver::disable();
        writeMainBufferCallbackId = -1;
        prepareCallbackId = -1;
    }
}


VkDescriptorSetLayout ShaderModel::createGraphicsDescriptorSetLayout()
{
    VkDescriptorSetLayout descriptorSetLayout;
    //binding id within shader
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    //allowed stages
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    //descriptor set
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if(vkCreateDescriptorSetLayout(vkbase::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
    return descriptorSetLayout;
}

VkDescriptorSetLayout ShaderModel::createComputeDescriptorSetLayout()
{
    VkDescriptorSetLayout descriptorSetLayout;
    //COMPUTE PIPELINE
    VkDescriptorSetLayoutBinding computeLayoutBindings[]
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        }
    };

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    computeLayoutInfo.bindingCount = std::size(computeLayoutBindings);
    computeLayoutInfo.pBindings = computeLayoutBindings;

    if(vkCreateDescriptorSetLayout(vkbase::device, &computeLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout for compute pipeline!");
    }
    return descriptorSetLayout;
}

void ShaderModel::createDescriptorSetLayout2()
{
    VkDescriptorSetLayoutBinding bindings[]
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT|VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = std::size(bindings);
    layoutInfo.pBindings = bindings;


    auto ds=vk::Device(vkbase::device).createDescriptorSetLayoutUnique(layoutInfo);
    descriptorSet2Layout=std::make_shared<vk::UniqueDescriptorSetLayout>(std::move(ds));
}

void ShaderModel::createDescriptorSets2()
{
    descriptorSets2.resize(vkbase::imageCount);
    std::vector<VkDescriptorSetLayout> layouts(vkbase::imageCount, descriptorSet2Layout->get());

    VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPoolStatic;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(vkbase::imageCount);
    allocInfo.pSetLayouts = layouts.data();

    if(vkAllocateDescriptorSets(vkbase::device, &allocInfo,
                                descriptorSets2.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets 2!");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = keysBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = keysBuffer.info.size;

    std::vector<VkWriteDescriptorSet> writes(vkbase::imageCount);
    for (int i=0;i<vkbase::imageCount;++i)
    {
        VkWriteDescriptorSet descriptorWrite{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descriptorWrite.dstSet = descriptorSets2[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional
        writes[i]=descriptorWrite;
    }

    vkUpdateDescriptorSets(vkbase::device, writes.size(), writes.data(), 0, nullptr);
}

void ShaderModel::createDescriptorPools()
{
    VkDescriptorPoolSize poolSizes[]{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
         VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1 + vkbase::imageCount + 10},
         VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1}
     };

    VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = static_cast<uint32_t>(vkbase::imageCount + 1 + 1);

    if(vkCreateDescriptorPool(vkbase::device, &poolInfo, nullptr, &descriptorPoolDynamic) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    VkDescriptorPoolSize poolSizesStatic[]{
         VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = vkbase::imageCount},
    };

    VkDescriptorPoolCreateInfo poolInfoStatic{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfoStatic.poolSizeCount = std::size(poolSizesStatic);
    poolInfoStatic.pPoolSizes = poolSizesStatic;
    poolInfoStatic.maxSets = static_cast<uint32_t>(vkbase::imageCount);

    if(vkCreateDescriptorPool(vkbase::device, &poolInfoStatic, nullptr, &descriptorPoolStatic) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void ShaderModel::createUniformBuffer()
{
    uniformBuffers.resize(vkbase::imageCount);

    for(auto &b: uniformBuffers)
        b.create(8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    parametersIntermediateBuffer.create(8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void ShaderModel::updateDescriptorSet(uint32_t i)
{
    //The information about buffer for descriptor set
    VkDescriptorBufferInfo bufferInfo{};
    //buffer
    bufferInfo.buffer = uniformBuffers[i].buffer;
    //the offset in buffer
    bufferInfo.offset = 0;
    //size of one set
    bufferInfo.range = uniformBuffers[i].info.size;

    VkWriteDescriptorSet descriptorWrite{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = descriptorSets[i];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    descriptorWrite.pImageInfo = nullptr; // Optional
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    vkUpdateDescriptorSets(vkbase::device, 1, &descriptorWrite, 0, nullptr);
}

void ShaderModel::updateComputeDescriptorSet()
{

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = parametersIntermediateBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = parametersIntermediateBuffer.info.size;

    VkWriteDescriptorSet descriptorWrites[]
    {
        VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = computeDescriptorSets,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo,
        .pTexelBufferView = nullptr,
        },
    };

    vkUpdateDescriptorSets(vkbase::device, std::size(descriptorWrites), descriptorWrites, 0, nullptr);
}

void ShaderModel::createDescriptorSets()
{
    vkResetDescriptorPool(vkbase::device, descriptorPoolDynamic, 0);
    //GRAPHICS PIPELINE
    //resize the array of descriptor on image count
    descriptorSets.resize(vkbase::imageCount);

    std::vector<VkDescriptorSetLayout> layouts(vkbase::imageCount, pipeline.descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPoolDynamic;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(vkbase::imageCount);
    allocInfo.pSetLayouts = layouts.data();


    //allocate an array of descriptor sets
    //the descriptor sets will be automatically freed with descriptor pool
    if(vkAllocateDescriptorSets(vkbase::device, &allocInfo,
                                descriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    //COMPUTE PIPELINE
    VkDescriptorSetAllocateInfo comp_allocInfo{};
    comp_allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    comp_allocInfo.descriptorPool = descriptorPoolDynamic;
    comp_allocInfo.descriptorSetCount = 1;
    comp_allocInfo.pSetLayouts = &pipeline.computeDescriptorSetLayout;

    if(vkAllocateDescriptorSets(vkbase::device, &comp_allocInfo,
                                &computeDescriptorSets) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets for compute pipeline!");
    }


    //update all descriptor sets
    for(size_t i = 0; i < vkbase::imageCount; ++i)
    {
        updateDescriptorSet(i);
    }

    updateComputeDescriptorSet();
}

void ShaderModel::createRenderBuffers()
{
    //command buffers for each frame
    cbRender.resize(vkbase::imageCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    //command pool from which command buffers will be allocated
    allocInfo.commandPool = commandPool;
    /*VK_COMMAND_BUFFER_LEVEL_PRIMARY: primary command buffers are the ones that can be submitted to a queue for execution.
    VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from the primary command buffers.*/
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    //number of command buffers to allocate
    allocInfo.commandBufferCount = (uint32_t) cbRender.size();

    //create command buffers
    if(vkAllocateCommandBuffers(vkbase::device, &allocInfo, cbRender.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    //    for (size_t i = 0; i < cbRender.size(); i++)
    //    {
    //        //create empty command buffers
    //        VkCommandBufferBeginInfo beginInfo{};
    //        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    //        auto inheritanceInfo =vkbase::createMainBufferInheritanceInfo(i);
    //        beginInfo.pInheritanceInfo = &inheritanceInfo;
    //
    //        if (vkBeginCommandBuffer(cbRender[i], &beginInfo) != VK_SUCCESS)
    //            throw std::runtime_error("failed to begin recording command buffer!");
    //
    //        vkEndCommandBuffer(cbRender[i]);
    //    }
}

void ShaderModel::createComputeCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkbase::commandPoolResetGraphics;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if(vkAllocateCommandBuffers(vkbase::device, &allocInfo, &cbCompute) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    if(vkBeginCommandBuffer(cbCompute, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording compute command buffer!");
    }
    vkEndCommandBuffer(cbCompute);
}

void ShaderModel::rewriteRenderCommands(int i)
{
    VkCommandBuffer cb = cbRender[i];
    VkCommandBufferInheritanceInfo inheritanceInfo = vkbase::createMainBufferInheritanceInfo(i);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT specifies that each recording of the command buffer will only be submitted once, and the command buffer will be reset and recorded again between each submission.
    //         VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT specifies that a secondary command buffer is considered to be entirely inside a render pass. If this is a primary command buffer, then this bit is ignored.
    //         VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT specifies that a command buffer can be resubmitted to a queue while it is in the pending state, and recorded into multiple primary command buffers.
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT; // Optional
    // for secondary buffer only (indicates which state to inherit from calling primary command buffer)
    beginInfo.pInheritanceInfo = &inheritanceInfo; // Optional

    if(vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    //render commands
    {
        //render area(pipeline uses dynamic state)
        VkViewport viewport{};
        viewport.x = viewArea.x * static_cast<float>(vkbase::extent.width);
        viewport.y = viewArea.y * static_cast<float>(vkbase::extent.height);
        viewport.width = static_cast<float>(vkbase::extent.width) * viewArea.z;
        viewport.height = static_cast<float>(vkbase::extent.height) * viewArea.w;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cb, 0, 1, &viewport);
        //scissor rectangle
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vkbase::extent;
        vkCmdSetScissor(cb, 0, 1, &scissor);
        //bind descriptor set
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline.pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                         pipeline.pipelineLayout, 1, 1, &descriptorSets2[i], 0, nullptr);
        //bind pipeline
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);
        //draw rectangle
        vkCmdDraw(cb, 3/*vertex count*/, 1/*instance count*/,
                  0/*offset gl_VertexIndex*/, 0/*(gl_InstanceIndex)*/);
    }
    vkEndCommandBuffer(cb);
}

void ShaderModel::rewriteComputeBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    if(vkBeginCommandBuffer(cbCompute, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    {
        vkCmdBindPipeline(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.computePipeline);
        vkCmdBindDescriptorSets(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.computePipelineLayout, 0, 1, &computeDescriptorSets, 0, nullptr);
        vkCmdBindDescriptorSets(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.computePipelineLayout, 1, 1, &descriptorSets2[0], 0, nullptr);
        vkCmdDispatch(cbCompute, 1, 1, 1);
    }

    vkEndCommandBuffer(cbCompute);
}

void ShaderModel::createComputeFence()
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;

    if(vkCreateFence(vkbase::device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create fence!");
    }
}

void ShaderModel::submitComputeBuffer()
{
    //copy data
    std::vector<char> params = uniformParameters.buildBuffer();
    memcpy(parametersIntermediateBuffer.map(), params.data(), params.size());
    parametersIntermediateBuffer.unmap();

    //copy keys
    memcpy(keysBuffer.map(), keys, sizeof(keys));
    keysBuffer.unmap();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cbCompute;

    vkQueueSubmit(vkbase::computeQueue, 1, &submitInfo, computeFence);

    vkWaitForFences(vkbase::device, 1, &computeFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkbase::device, 1, &computeFence);


    //copy data back
    uniformParameters.readFromBuffer(parametersIntermediateBuffer.map());
    parametersIntermediateBuffer.unmap();
}

void ShaderModel::rewriteAllCommandBuffers()
{
    if(pipeline.graphicsPipeline&&pipeline.computePipeline)
    {
        vkDeviceWaitIdle(vkbase::device);
        vkResetCommandPool(vkbase::device, commandPool, 0);
        for(int i = 0; i < cbRender.size(); i++)
        {
            rewriteRenderCommands(i);
        }

        rewriteComputeBuffer();
        cbRenderValid = true;
    }
    else
    {
        cbRenderValid = false;
    }
}


VkPipelineLayout ShaderModel::createGraphicsPipelineLayout(const VkDescriptorSetLayout& descriptorSetLayout, const VkDescriptorSetLayout& descriptorSet2Layout)
{
    VkDescriptorSetLayout setLayouts[]
    {
        descriptorSetLayout,
        descriptorSet2Layout
    };

    VkPipelineLayout layout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //count descriptor sets layout
    pipelineLayoutInfo.setLayoutCount = std::size(setLayouts);
    //array of descriptor set layout's
    pipelineLayoutInfo.pSetLayouts = setLayouts; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if(vkCreatePipelineLayout(vkbase::device, &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    return layout;
}

VkPipelineLayout ShaderModel::createComputePipelineLayout(const VkDescriptorSetLayout& descriptorSetLayout, VkDescriptorSetLayout descriptorSet2Layout)
{
    VkDescriptorSetLayout setLayouts[]
    {
        descriptorSetLayout,
        descriptorSet2Layout
    };

    VkPipelineLayout layout;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = std::size(setLayouts);
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts;

    if (vkCreatePipelineLayout(vkbase::device, &pipelineLayoutCreateInfo, nullptr, &layout)!= VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    return layout;
}

void ShaderModel::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    //queue which will be used to submit command buffers to
    poolInfo.queueFamilyIndex = vkbase::queueFamilyIndices.graphicsFamily.value();
    //possible flags:
    //VK_COMMAND_POOL_CREATE_TRANSIENT_BIT specifies that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe. This flag may be used by the implementation to control memory allocation behavior within the pool.
    //VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from a pool to be individually reset to the initial state; either by calling vkResetCommandBuffer, or via the implicit reset when calling vkBeginCommandBuffer. If this flag is not set on a pool, then vkResetCommandBuffer must not be called for any command buffer allocated from that pool.
    //VK_COMMAND_POOL_CREATE_PROTECTED_BIT specifies that command buffers allocated from the pool are protected command buffers.
    poolInfo.flags = 0;

    //create poll (command poll will be destroyed in cleanup function)
    if(vkCreateCommandPool(vkbase::device, &poolInfo, nullptr, &commandPool/*out handle*/) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool!");
}

VkPipeline ShaderModel::createGraphicsPipeline(const std::vector<uint32_t> &frag_shader, VkPipelineLayout layout)
{
    //create shader modules
    VkShaderModule vertShaderModule = vkbase::createShaderModule(vkbase::assets::get( "shaders/vert.spv"));
    VkShaderModule fragShaderModule = vkbase::createShaderModule(frag_shader);


    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";


    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};


    //vertex input (empty)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;


    //Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    /*Topology, one of: VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP*/
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;


    //--VIEWPORT--//
    //viewport and scissor (dynamic state)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    //--RASTERIZATION--//
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    //depth basis
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional


    //--MULTISAMPLING--//(disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional


    //--BLENDING--//
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional


    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional


    //--DYNAMIC STATES--//
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = std::size(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;


    //--PIPELINE--//
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    //shaders
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    //vertex input
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    //draw mode
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    //viewport (dynamic)
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState; // Optional
    //uniform
    pipelineInfo.layout = layout;
    //render pass describes where and how to update image
    pipelineInfo.renderPass = vkbase::renderPass;
    //subpass in render pass (only one)
    pipelineInfo.subpass = 0;
    //allows to copy state from existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    //CREATE PIPELINE
    VkPipeline newPipeline;
    auto result = vkCreateGraphicsPipelines(vkbase::device, VK_NULL_HANDLE, 1, &pipelineInfo,nullptr,
                              &newPipeline);

    //destroy shader modules
    vkDestroyShaderModule(vkbase::device, fragShaderModule, nullptr);
    vkDestroyShaderModule(vkbase::device, vertShaderModule, nullptr);

    if(result != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }


    return newPipeline;
}

VkPipeline ShaderModel::createComputePipeline(const std::vector<uint32_t> &comp_shader, VkPipelineLayout layout)
{
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = vkbase::createShaderModule(comp_shader);
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = layout;
    pipelineInfo.flags = 0;

    VkPipeline newPipeline;
    auto result = vkCreateComputePipelines(vkbase::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);

    //destroy shader module
    vkDestroyShaderModule(vkbase::device, pipelineInfo.stage.module, nullptr);

    if(result != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return newPipeline;
}

void ShaderModel::onSurfaceChanged()
{
    recreateSurfaceDependentObjects();
    syncCompilation();
    updateCommandBuffers = true;
}

void ShaderModel::prepare([[maybe_unused]] uint32_t image_index)
{
    compilationTaskManager.finish();
    if (!slang_project.modifiedFiles.empty())
    {
        recompileFlag=true;
        slang_project.modifiedFiles.clear();
        slang_project.updateDependencies(m_source);
    }

    recompile();

    if(updateCommandBuffers)
    {
        rewriteAllCommandBuffers();
        updateCommandBuffers = false;
    }

    submitComputeBuffer();
}

void ShaderModel::onUpdateData(uint32_t imageIndex)
{
    updateUniform(imageIndex);
}

void ShaderModel::writeCommandBuffer(VkCommandBuffer cbMain, uint32_t imageIndex)
{
    if(cbRenderValid)
    {
        vkCmdExecuteCommands(cbMain, 1, &cbRender[imageIndex]);
    }
}


void ShaderModel::updateUniform(uint32_t imageIndex)
{
    std::vector<char> params = uniformParameters.buildBuffer();
    memcpy(uniformBuffers[imageIndex].map(), params.data(), params.size());
    uniformBuffers[imageIndex].unmap();
}

void ShaderModel::destroy()
{
    setActive(false);
    vkDeviceWaitIdle(vkbase::device);
    for(auto &b: uniformBuffers)
        b.destroy();

    vkDestroyDescriptorPool(vkbase::device, descriptorPoolDynamic, nullptr);
    vkDestroyDescriptorPool(vkbase::device, descriptorPoolStatic, nullptr);
    vkDestroyFence(vkbase::device, computeFence, nullptr);
    parametersIntermediateBuffer.destroy();
    keysBuffer.destroy();

    vkDestroyCommandPool(vkbase::device, commandPool, nullptr);
}

ShaderModel::~ShaderModel()
{
    destroy();
    compilationTaskManager.detachAll();
}

std::string ShaderModel::exportOutput(const std::string &funcName, const std::string &language, bool onlyBody)
{
    return "";
}

void ShaderModel::generateBitmap(int width, int height, const std::string &functionName, const std::string &functionCall, VkDescriptorSetLayout descriptorSetLayout)
{
    auto hlslExportFunction = exportOutput(functionName, "HLSL", false);
    bitmapGenerator.generateBitmap(hlslExportFunction, functionCall, width, height, descriptorPoolDynamic, descriptorSetLayout);
}

VkDescriptorSet ShaderModel::getBitmapDescriptorSet()
{
    return bitmapGenerator.getDescriptorSet();
}

std::vector<uint32_t> ShaderModel::getBitmap()
{
    return bitmapGenerator.getBitmap();
}

glm::vec2 ShaderModel::getBitmapSize()
{
    return {bitmapGenerator.getWidth(), bitmapGenerator.getHeight()};
}

const BitmapGenerator &ShaderModel::getBitmapGenerator()
{
    return bitmapGenerator;
}

const std::string& ShaderModel::get_error()
{
    return fragment_shader.diagnostics_text;
}

const vkbase::ShadersRC::CompilationResult ShaderModel::get_frag_result() const
{
    return fragment_shader;
}

const vkbase::ShadersRC::CompilationResult ShaderModel::get_compute_result() const
{
    return compute_shader;
}

bool ShaderModel::isCompiling() const
{
    return status==Status::COMPILING;
}

