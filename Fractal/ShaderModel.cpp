#include <fstream>
#include <iostream>
#include "ShaderModel.hpp"
#include "GLSLCompiler.h"
#include "HLSLCompiler.h"
#include "SPIRVCompiler.h"
#include <spirv_reflect.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <variant>

class NotConvertible
{
};

using ParameterToConvert = std::variant<NotConvertible *, int *, glm::ivec2 *, glm::ivec3 *, glm::ivec4 *,
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
    return reinterpret_cast<NotConvertible *>(0);
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
        size = j["ssbos"][0]["block_size"];
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
            if(p.type == activeParameters[name].type)//copy if possible
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

//int vecCount(std::string typePrefix, std::string typeName)
//{
//    if(typeName.find(typePrefix)==0)
//    {
//        std::string std::stoi(typeName.substr(typePrefix.size()));
//    }
//    return 0;
//}

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
    compilers.emplace_back(new GLSLCompiler());
    currentCompiler = compilers[0].get();
    currentCompiler->shaderName = this->name + ".frag";

    keysBuffer.create(sizeof(keys), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    createDescriptorSetLayout();
    createUniformBuffer();
    createDescriptorPool();
    createDescriptorSets();
    createPipelineLayout();
    createCommandPool();
    createRenderBuffers();
    createComputeFence();
    createComputeCommandBuffer();
    updateBuffers = true;

    //sync parameters
    for(int i = 0; i < vkbase::imageCount; i++)
    {
        updateUniform(i);
    }
    vkbase::OnDataUpdateReceiver::disable();
}


void ShaderModel::setSource(const std::string &source)
{
    currentCompiler->setSource(source);
    recomplile = true;

}

std::string ShaderModel::getSource()
{
    return currentCompiler->getSource();
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
            std::cout << varName << std::endl;
        }

        std::string globalVar = shaderCode.substr(i, end - i + 1);
        if(globalVar!="static float4 _entryPointOutput;")
            constants += "\n    " + globalVar;
        i = end;
    }


    std::string funcBody = shaderCode.substr(beginPos, endPos - beginPos);
    std::string entryPointOutput = "_entryPointOutput = ";
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
    if(reflection["inputs"].size() > 0)
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

    std::map<std::string , std::string> defines;
    if(!outGLSL)
        defines["mod(a,b)"]="(a-floor(a/b)*b)";

    return vkbase::ShadersRC::preprocessShaderHLSL(func, "output.frag", vkbase::ShadersRC::ShaderType::Fragment, defines);
}


void ShaderModel::updateBin()
{
    if(currentCompiler && threadsCount < MAX_THREADS)
    {
        ++threadsCount;
        int id = ++lastThreadID;
        std::thread([this, id]()
                    {
                        status = COMPILING;
                        std::cout << "+Compilation started:" << name << "(" << id << ")\n";
                        std::vector<uint32_t> bin;
                        std::vector<uint32_t> computeBin;
                        try
                        {
                            currentCompiler->shaderName = name;
                            bin = currentCompiler->compile();
                            computeBin = currentCompiler->compileCompute();
                        }
                        catch(const std::exception &e)
                        {
                            shaderMutex.lock();
                            errorMessage = e.what();
                            status = ERROR;
                            --threadsCount;
                            shaderMutex.unlock();
                            return;
                        }

                        std::cout << "-Compilation finished:" << name << "(" << id << ")\n";
                        if(id == lastThreadID)//update if thread is not canceled (only if last thread in queue, otherwise ignore)
                        {
                            spirv_cross::CompilerReflection compilerReflection(bin);
                            auto reflection = compilerReflection.compile();
                            // std::cout<<reflection;

                            try
                            {
                                spirv_cross::CompilerReflection compilerComputeReflection(computeBin);
                                auto reflectionC = compilerComputeReflection.compile();
                                // std::cout << reflectionC;
                            }
                            catch(const std::exception &e)
                            {
                                errorMessage = e.what();
                                status = ERROR;
                                --threadsCount;
                                return;
                            }


                            shaderMutex.lock();
                            newUniformParametersReflection = reflection;
                            shaderBin = std::move(bin);
                            computeShaderBin = std::move(computeBin);
                            isChanged = true;
                            status = COMPILED;
                            errorMessage = "";
                            shaderMutex.unlock();
                            std::cout << "=Binary code updated:" << name << "(" << id << ")\n";
                        }
                        --threadsCount;
                    }).detach();
    }
}


[[maybe_unused]] void ShaderModel::updateTranslation(AbstractShaderCompiler *compiler)
{
    compiler->setSource(compiler->getSourceFromOther(*currentCompiler));
}

AbstractShaderCompiler *ShaderModel::getCompilerByLanguage(const std::string &languageName)
{
    for(auto &compiler: compilers)
    {
        if(compiler->languageName == languageName)
            return compiler.get();
    }
    return nullptr;
}

void ShaderModel::setCurrentCompiler(const std::string &languageName)
{
    currentCompiler = getCompilerByLanguage(languageName);
}


nlohmann::json ShaderModel::toJson()
{
    nlohmann::json json;
    json["name"] = name;
    json["compilers"] = nlohmann::json::array();
    for(auto &compiler: compilers)
    {
        nlohmann::json compilerJson;
        compilerJson["shaderName"] = compiler->shaderName;
        compilerJson["languageName"] = compiler->languageName;
        compilerJson["source"] = compiler->getSource();
        compilerJson["isCurrent"] = compiler.get() == currentCompiler;
        json["compilers"].push_back(compilerJson);
        json["parameters"] = uniformParameters.serialize();
    }
    return json;
}

void ShaderModel::loadFromJson(const nlohmann::json &json)
{
    name = json["name"];
    compilers.clear();
    compilers.reserve(json["compilers"].size());
    currentCompiler = nullptr;

    for(auto &compilerJson: json["compilers"])
    {
        std::string shaderName = compilerJson["shaderName"];
        std::string languageName = compilerJson["languageName"];
        std::string source = compilerJson["source"];
        if(languageName != "HLSL")
            continue;

        if(languageName == "GLSL")
            compilers.emplace_back(new GLSLCompiler());
        else if(languageName == "HLSL")
            compilers.emplace_back(new HLSLCompiler());
        else if(languageName == "SPIRV")
            compilers.emplace_back(new SPIRVCompiler());
        else
            throw std::runtime_error("Unknown shader language: " + languageName);

        if(compilerJson["isCurrent"])
            currentCompiler = compilers.back().get();
        compilers.back()->shaderName = shaderName;
        compilers.back()->languageName = languageName;
        compilers.back()->setSource(source);
        if(!currentCompiler)
            currentCompiler = compilers.back().get();
        if(json.contains("parameters"))
            uniformParameters.deserialize(json["parameters"]);
    }
    recomplile = true;
}

void ShaderModel::setViewArea(glm::vec4 newArea)
{
    if(newArea != viewArea)
    {
        viewArea = newArea;
        updateBuffers = true;
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
        prepareCallbackId = vkbase::addDrawPrepareCallback(WRAP_MEMBER_FUNC(prepare));
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

void ShaderModel::createDescriptorSetLayout()
{
    //GRAPHICS PIPELINE
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

    //COMPUTE PIPELINE
    VkDescriptorSetLayoutBinding computeLayoutBindings[2]{};
    computeLayoutBindings[0].binding = 0;
    computeLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeLayoutBindings[0].descriptorCount = 1;
    computeLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeLayoutBindings[0].pImmutableSamplers = nullptr; // Optional

    computeLayoutBindings[1].binding = 1;
    computeLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    computeLayoutBindings[1].descriptorCount = 1;
    computeLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeLayoutInfo.bindingCount = std::size(computeLayoutBindings);
    computeLayoutInfo.pBindings = computeLayoutBindings;

    if(vkCreateDescriptorSetLayout(vkbase::device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout for compute pipeline!");
    }
}

void ShaderModel::createDescriptorPool()
{
    VkDescriptorPoolSize poolSizes[3]{};
    //graphics pipeline
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    //compute pipeline
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 1 + vkbase::imageCount + 10;
    //
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 1;


    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = static_cast<uint32_t>(vkbase::imageCount + 1 + 1);

    if(vkCreateDescriptorPool(vkbase::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
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

    VkWriteDescriptorSet descriptorWrite{};
    //type
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
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
    VkWriteDescriptorSet descriptorWrites[2]{};

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = parametersIntermediateBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = parametersIntermediateBuffer.info.size;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = computeDescriptorSets;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;
    descriptorWrites[0].pImageInfo = nullptr;
    descriptorWrites[0].pTexelBufferView = nullptr;

    VkDescriptorBufferInfo bufferInfoKeys{};
    bufferInfoKeys.buffer = keysBuffer.buffer;
    bufferInfoKeys.offset = 0;
    bufferInfoKeys.range = keysBuffer.info.size;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = computeDescriptorSets;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfoKeys;
    descriptorWrites[1].pImageInfo = nullptr;
    descriptorWrites[1].pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(vkbase::device, std::size(descriptorWrites), descriptorWrites, 0, nullptr);
}

void ShaderModel::createDescriptorSets()
{
    //GRAPHICS PIPELINE

    //resize the array of descriptor on image count
    descriptorSets.resize(vkbase::imageCount);

    std::vector<VkDescriptorSetLayout> layouts(vkbase::imageCount, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
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
    comp_allocInfo.descriptorPool = descriptorPool;
    comp_allocInfo.descriptorSetCount = 1;
    comp_allocInfo.pSetLayouts = &computeDescriptorSetLayout;

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
    allocInfo.commandPool = vkbase::commandPoolReset;
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

void ShaderModel::rewriteRenderBuffer(int i)
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
                                pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
        //bind pipeline
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        //draw rectangle
        vkCmdDraw(cb, 4/*vertex count*/, 1/*instance count*/,
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
        vkCmdBindPipeline(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(cbCompute, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets, 0, nullptr);
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
    if(graphicsPipeline)
    {
        vkDeviceWaitIdle(vkbase::device);
        vkResetCommandPool(vkbase::device, commandPool, 0);
        for(int i = 0; i < cbRender.size(); i++)
        {
            rewriteRenderBuffer(i);
        }

        rewriteComputeBuffer();
        cbRenderValid = true;
    }
}

void ShaderModel::createPipelineLayout()
{
    //--GRAPHIC PIPELINE LAYOUT--//
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //count descriptor sets layout
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    //array of descriptor set layout's
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if(vkCreatePipelineLayout(vkbase::device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    //--COMPUTE PIPELINE LAYOUT--//
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &computeDescriptorSetLayout;
    vkCreatePipelineLayout(vkbase::device, &pipelineLayoutCreateInfo, nullptr, &computePipelineLayout);

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

void ShaderModel::createGraphicsPipeline(const std::vector<uint32_t> &frag_shader)
{
    //create shader modules
    VkShaderModule vertShaderModule = vkbase::loadPrecompiledShader("shaders/VERT_shader.vert");
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
    pipelineInfo.layout = pipelineLayout;
    //render pass describes where and how to update image
    pipelineInfo.renderPass = vkbase::renderPass;
    //subpass in render pass (only one)
    pipelineInfo.subpass = 0;
    //allows to copy state from existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional


    VkPipeline newPipeline;
    //CREATE PIPELINE
    if(vkCreateGraphicsPipelines(vkbase::device,
                                 VK_NULL_HANDLE,//cache
                                 1,//count
                                 &pipelineInfo,
                                 nullptr,
                                 &newPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    //modules may be destroyed now
    vkDestroyShaderModule(vkbase::device, fragShaderModule, nullptr);
    vkDestroyShaderModule(vkbase::device, vertShaderModule, nullptr);

    //Replace old pipeline
    if(graphicsPipeline)
    {
        vkDeviceWaitIdle(vkbase::device);
        vkDestroyPipeline(vkbase::device, graphicsPipeline, nullptr);
    }
    graphicsPipeline = newPipeline;
}

void ShaderModel::createComputePipeline(const std::vector<uint32_t> &comp_shader)
{
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = vkbase::createShaderModule(comp_shader);
    pipelineInfo.stage.pName = "___update___";
    pipelineInfo.layout = computePipelineLayout;
    pipelineInfo.flags = 0;

    if(computePipeline)
    {
        vkDestroyPipeline(vkbase::device, computePipeline, nullptr);
    }

    if(vkCreateComputePipelines(vkbase::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline ");

    //destroy shader module
    vkDestroyShaderModule(vkbase::device, pipelineInfo.stage.module, nullptr);
}

void ShaderModel::onSurfaceChanged()
{
    updateBuffers = true;
}

void ShaderModel::prepare([[maybe_unused]] uint32_t image_index)
{
    syncShaderCode();

    if(recomplile)
    {
        updateBin();
        recomplile = false;
    }

    if(updateBuffers)
    {
        rewriteAllCommandBuffers();
        updateBuffers = false;
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

void ShaderModel::syncShaderCode()
{
    if(isChanged)
    {
        shaderMutex.lock();
        if(!shaderBin.empty())
        {
            //update parameters
            uniformParameters = UniformParameters(uniformParameters, newUniformParametersReflection);

            vkDeviceWaitIdle(vkbase::device);

            //update buffers and descriptors
            if(uniformParameters.size > uniformBuffers[0].info.size)
                for(int i = 0; i < vkbase::imageCount; ++i)
                {
                    if(uniformBuffers[i].info.size < uniformParameters.size)
                    {
                        uniformBuffers[i].create(uniformParameters.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                        parametersIntermediateBuffer.create(uniformParameters.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                        updateDescriptorSet(i);
                        updateComputeDescriptorSet();
                    }
                }

            //update pipelines
            createGraphicsPipeline(shaderBin);
            createComputePipeline(computeShaderBin);

            updateBuffers = true;
            isChanged = false;

        }
        shaderMutex.unlock();
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

    vkDestroyDescriptorSetLayout(vkbase::device, descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(vkbase::device, pipelineLayout, nullptr);
    vkDestroyPipeline(vkbase::device, graphicsPipeline, nullptr);
    vkDestroyDescriptorPool(vkbase::device, descriptorPool, nullptr);

    vkDestroyPipelineLayout(vkbase::device, computePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(vkbase::device, computeDescriptorSetLayout, nullptr);
    vkDestroyPipeline(vkbase::device, computePipeline, nullptr);
    vkDestroyFence(vkbase::device, computeFence, nullptr);
    parametersIntermediateBuffer.destroy();
    keysBuffer.destroy();

    vkDestroyCommandPool(vkbase::device, commandPool, nullptr);
}

ShaderModel::~ShaderModel()
{
    destroy();
}

std::string ShaderModel::exportOutput(const std::string &funcName, const std::string &language, bool onlyBody)
{
    if(language == "GLSL")
        return exportFunction(currentCompiler->compileForExport("output", uniformParameters.dynamicParametersString(), uniformParameters.initStructureString("p")), funcName, onlyBody, true);
    else if(language == "HLSL")
        return exportFunction(currentCompiler->compileForExport("output", uniformParameters.dynamicParametersString(), uniformParameters.initStructureString("p")), funcName, onlyBody, false);
    else
        return "Unknown language: " + language;
}

void ShaderModel::generateBitmap(int width, int height, const std::string &functionName, const std::string &functionCall, VkDescriptorSetLayout descriptorSetLayout)
{
    auto hlslExportFunction = exportOutput(functionName, "HLSL", false);
    bitmapGenerator.generateBitmap(hlslExportFunction, functionCall, width, height, descriptorPool, descriptorSetLayout);
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

