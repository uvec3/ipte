#pragma once
#include <string>
#include <vector>
#include <map>
#include "../External/json.hpp"
#include "../vkbase/extensions/ShadersRC/dxc/ShadersDXC.hpp"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <unordered_map>
#include "StructField.hpp"
#include "../vkbase/extensions/Buffer/Buffer.hpp"

class NonConvertible{};

using ParameterToConvert = std::variant<NonConvertible *, int *, glm::ivec2 *, glm::ivec3 *, glm::ivec4 *,
                                        float *, glm::vec2 *, glm::vec3 *, glm::vec4 *,
                                        double *, glm::dvec2 *, glm::dvec3 *, glm::dvec4 *,
                                        uint32_t *, glm::uvec2 *, glm::uvec3 *, glm::uvec4 *>;

ParameterToConvert getConvertable(const std::string& type,void* data);
bool convert(ParameterToConvert &from, ParameterToConvert &to);

struct CachedParameter
{
    std::string type;
    std::vector<int> data;
};

typedef std::shared_ptr<std::map<std::string,CachedParameter>> ParametersCache;

struct Binding
{
    uint32_t id;
    StructField struct_field;
};


struct ParametersBuffers
{
    ParametersBuffers()=default;
    ParametersBuffers(const ParametersBuffers&)=delete;
    ParametersBuffers(ParametersBuffers&&) noexcept =default;
    ParametersBuffers& operator=(const ParametersBuffers&)=delete;
    ParametersBuffers& operator=(ParametersBuffers&&)=default;

    std::unordered_map<uint32_t,vkbase::Buffer> buffers;


    static void moveBuffersCMD(VkCommandBuffer cmd,ParametersBuffers& src, ParametersBuffers& dst);
};



std::string glslToHlsl(const std::string &glslT);

class ParametersDescriptor
{
    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

class UniformParameter
{
public:
    std::string name;
    std::string type;
    int offset=0;
    std::vector<char> data;

    bool isDynamic = false;
    bool array=false;
    nlohmann::json metadata;

    // template<typename T>
    // T& get()
    // {
    //     return *reinterpret_cast<T*>(data.data());
    // }
    //
    // int size() const
    // {
    //     return static_cast<int>(data.size());
    // }
};
class UniformParameters
{
    ParametersCache cache;

public:

    std::shared_ptr<ParametersBuffers> localBuffers;
    std::unordered_map<int,Binding> bindings;

    UniformParameters()= default;
    UniformParameters (const UniformParameters& other)=default;
    UniformParameters& operator=(UniformParameters const& other)=default;
    std::string initC(const std::string& suffix="");

    UniformParameters(UniformParameters oldParameters, const vkbase::rc::Spirv &spirv);
    std::string dynamicParametersString();
    ParametersBuffers createBuffers(VkBufferUsageFlags usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VmaAllocationCreateFlags flags=0);


    Binding* getGroup(const std::string& name);

    template<typename T>
    bool setParameterIfExists(const std::string& name, T value)
    {
        bool result =false;
        for (auto& [b_id,binding]:bindings)
        {
            auto struct_ptr=binding.struct_field.find(name);
            if (struct_ptr)
            {
                void* data=localBuffers->buffers[b_id].map()+struct_ptr->offset;
                if constexpr(std::is_assignable_v<ParameterToConvert,T*>)
                {
                    auto convertable_to=getConvertable(struct_ptr->data_type,data);
                    if (!std::holds_alternative<NonConvertible*>(convertable_to))
                    {
                        ParameterToConvert convertable_from=&value;
                        convert(convertable_from,convertable_to);
                        result=true;
                    }
                }
                else if constexpr(std::is_assignable_v<std::vector<char>,T>)
                {
                    std::span span=value;
                    if (span.size()==struct_ptr->size)
                    {
                        std::memcpy(data,span.data(),span.size());
                    }
                }

                localBuffers->buffers[b_id].unmap();
            }
        }
        return result;
    }

    vk::DescriptorSetLayout createDescriptorSetLayout();
    void uploadBuffers(ParametersBuffers& buffers);
    void downloadBuffers(ParametersBuffers& buffers);
    static void updateDescriptorSet(VkDescriptorSet& ds, const ParametersBuffers& buffers);


    friend void to_json(nlohmann::json& j, const UniformParameters& field);

    friend inline void from_json(const nlohmann::json& j, UniformParameters& field)
    {
        const auto& src = j;
        auto& target = field;

        target.bindings.clear();

        if (j.contains("members"))
        {
            auto size = src.value("size", 0);
            for(auto &[name, p]: j["members"].items())
            {

                std::vector<char> data=p["data"];
                StructField s;
                s.name=name;
                s.data_type=p["type"];
                s.base_data_type=s.data_type;
                s.offset=p["offset"];
                s.size=data.size();

                target.bindings[0].struct_field.sub_fields->insert(s);
            }
            target.bindings[0].struct_field.size=size;

            target.localBuffers = std::make_shared<ParametersBuffers>(
                    target.createBuffers(VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT));

            char* buffer=target.localBuffers->buffers[0].map();
            for(auto &[name, p]: j["members"].items())
            {
                std::vector<char> data=p["data"];
                target.setParameterIfExists(name,data);
            }
            target.localBuffers->buffers[0].unmap();

            return;;
        }


        if (auto it = src.find("bindings"); it != src.end() && it->is_array())
        {
            for (const auto& binding_json : *it)
            {
                Binding binding;
                binding.id = binding_json.value("id", 0);
                if (auto sf = binding_json.find("struct_field"); sf != binding_json.end())
                {
                    binding.struct_field = sf->get<StructField>();
                }
                target.bindings[binding.id] = binding;
            }
        }

        target.localBuffers = std::make_shared<ParametersBuffers>(
            target.createBuffers(VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT));

        if (auto it = src.find("bindings"); it != src.end() && it->is_array())
        {
            for (const auto& binding_json : *it)
            {
                const int binding_id = binding_json.value("id", 0);
                if (!target.localBuffers->buffers.contains(binding_id))
                {
                    continue;
                }

                auto& buffer = target.localBuffers->buffers.at(binding_id);
                auto data_it = binding_json.find("data");
                if (data_it == binding_json.end() || !data_it->is_array())
                {
                    continue;
                }

                std::vector<int> bytes = data_it->get<std::vector<int>>();
                const auto buffer_size = static_cast<size_t>(buffer.size());
                const size_t copy_size = bytes.size() < buffer_size ? bytes.size() : buffer_size;

                char* ptr = buffer.map();
                for (size_t i = 0; i < copy_size; ++i)
                {
                    ptr[i] = static_cast<char>(static_cast<unsigned char>(bytes[i]));
                }
                buffer.unmap();
            }
        }
    }
};