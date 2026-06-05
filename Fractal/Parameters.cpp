#include "Parameters.hpp"
#include <iostream>
#include <variant>
#include "glm/glm.hpp"
#include "../External/json.hpp"
#include "../vkbase/core/EngineBase.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"



ParameterToConvert getConvertable(const std::string& type,void* data)
{
    if(type == "int")
        return reinterpret_cast<int *>(data);
    if(type == "ivec2")
        return reinterpret_cast<glm::ivec2 *>(data);
    if(type == "ivec3")
        return reinterpret_cast<glm::ivec3 *>(data);
    if(type == "ivec4")
        return reinterpret_cast<glm::ivec4 *>(data);
    if(type == "float")
        return reinterpret_cast<float *>(data);
    if(type == "float2")
        return reinterpret_cast<glm::vec2 *>(data);
    if(type == "float3")
        return reinterpret_cast<glm::vec3 *>(data);
    if(type == "float4")
        return reinterpret_cast<glm::vec4 *>(data);
    if(type == "double")
        return reinterpret_cast<double *>(data);
    if(type == "double2")
        return reinterpret_cast<glm::dvec2 *>(data);
    if(type == "double3")
        return reinterpret_cast<glm::dvec3 *>(data);
    if(type == "double4")
        return reinterpret_cast<glm::dvec4 *>(data);
    if(type == "uint")
        return reinterpret_cast<uint32_t *>(data);
    if(type == "uint2")
        return reinterpret_cast<glm::uvec2 *>(data);
    if(type == "uint3")
        return reinterpret_cast<glm::uvec3 *>(data);
    if(type == "uint4")
        return reinterpret_cast<glm::uvec4 *>(data);
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


std::string UniformParameters::initC(const std::string& suffix)
{
    std::string result;
    for (auto& [b_id,binding]:bindings)
    {
        char* data= localBuffers->buffers[b_id].map();
        result+=binding.struct_field.initDataC(data, suffix);
        localBuffers->buffers[b_id].unmap();
    }
    return  result;
}

UniformParameters::UniformParameters(UniformParameters oldParameters,const vkbase::rc::Spirv &spirv)
{
    vkbase::rc::SPIRVReflection reflection(spirv);
    SpvReflectDescriptorSet* set=nullptr;
    for (auto s : reflection.descriptorSets)
    {
        if (s->set==0)
        {
            set=s;
            break;;
        }
    }
    if (set!=nullptr)
    {
        for (auto i =0;i<set->binding_count;++i)
        {
            auto b=set->bindings[i];
            StructField field=StructField::readStructField(&(b->block));
            bindings[b->binding].struct_field=field;
            bindings[b->binding].id=b->binding;
        }
    }

    localBuffers=std::make_shared<ParametersBuffers>(createBuffers(VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT));


    //transfer old data
    for (auto& b:bindings|std::views::values)
    {
        auto old_b=oldParameters.getGroup(b.struct_field.name);
        if (!old_b)
        {
            auto& same_binding = oldParameters.bindings[b.id];
            if (same_binding.struct_field.size==b.struct_field.size)
            {
                old_b=&same_binding;
            }
        }

        if (old_b)
        {
            translateData(old_b->struct_field,b.struct_field,
                oldParameters.localBuffers->buffers[old_b->id].map(), localBuffers->buffers[b.id].map());

            localBuffers->buffers[b.id].unmap();
            oldParameters.localBuffers->buffers[old_b->id].unmap();
        }
    }
}


std::string UniformParameters::dynamicParametersString()
{
    std::stringstream ss;
    std::string sep=" ";
    // for(auto &[name, p]: activeParameters)
    // {
    //     if(p.isDynamic)
    //     {
    //         ss <<sep << glslToHlsl(p.type) << " _arg_" << name;
    //         sep= ", ";
    //     }
    // }

    return ss.str();
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

void ParametersBuffers::moveBuffersCMD(VkCommandBuffer cmd, ParametersBuffers& src, ParametersBuffers& dst)
{
    assert(src.buffers.size()==dst.buffers.size());
    for (int i=0;i<src.buffers.size();++i)
    {
        src.buffers[i].copy_to_buffer_cmd(cmd,dst.buffers[i]);
    }
}

std::string glslToHlsl(const std::string &glslT)
{
    if(glslToHlslType.contains(glslT))
        return glslToHlslType[glslT];
    return glslT;
}


vk::DescriptorSetLayout UniformParameters::createDescriptorSetLayout()
{
    std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
    layout_bindings.reserve(bindings.size());
    for (auto& [binding_id,binding]:bindings)
    {
        VkDescriptorSetLayoutBinding b;
        b.binding=binding_id;
        b.descriptorCount=1;
        b.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings.push_back(b);
    }

    //descriptor set
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = layout_bindings.size();
    layoutInfo.pBindings = layout_bindings.data();

    return vkbase::device.createDescriptorSetLayout(layoutInfo);
}

ParametersBuffers UniformParameters::createBuffers(VkBufferUsageFlags usage, VmaAllocationCreateFlags flags)
{
    ParametersBuffers result;
    result.buffers.reserve(bindings.size());
    for (const auto& [b_id,binding]:bindings)
    {
        result.buffers[b_id]=vkbase::Buffer(binding.struct_field.size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT|usage,flags);
    }
    return  result;
}

Binding* UniformParameters::getGroup(const std::string& name)
{
    for (auto& b:bindings|std::views::values)
    {
        if (b.struct_field.name==name)
            return  &b;
    }
    return nullptr;
}

void UniformParameters::updateDescriptorSet(VkDescriptorSet& ds, const ParametersBuffers& buffers)
{
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(buffers.buffers.size());//important to reserve here so addresses won't change
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(buffers.buffers.size());


    for (auto& [b_id,buffer]:buffers.buffers)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = buffer.size();
        buffer_infos.push_back(bufferInfo);


        writes.emplace_back(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = b_id,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &buffer_infos.back(),
            .pTexelBufferView = nullptr,
            });
    }

    vkUpdateDescriptorSets(vkbase::device, writes.size(), writes.data(), 0, nullptr);
}

void to_json(nlohmann::json& j, const UniformParameters& field)
{
    j = nlohmann::json{
        {"bindings", nlohmann::json::array()},
    };

    for (const auto& [binding_id, binding] : field.bindings)
    {
        nlohmann::json binding_json{
            {"id", binding_id},
            {"struct_field", binding.struct_field},
        };

        if (field.localBuffers && field.localBuffers->buffers.contains(binding_id))
        {
            auto& buffer = field.localBuffers->buffers.at(binding_id);
            const auto buffer_size = static_cast<size_t>(buffer.size());
            std::vector<int> bytes;
            bytes.resize(buffer_size);

            char* ptr = buffer.map();
            for (size_t i = 0; i < buffer_size; ++i)
            {
                bytes[i] = static_cast<unsigned char>(ptr[i]);
            }
            buffer.unmap();

            binding_json["data"] = std::move(bytes);
        }
        else
        {
            binding_json["data"] = nlohmann::json::array();
        }

        j["bindings"].push_back(std::move(binding_json));
    }
}
