#pragma once
#include <string>
#include <vector>
#include <memory>
#include <span>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/tag.hpp>
#include "../External/json.hpp"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"


struct StructFieldByName;
struct StructFieldByInsertionOrder;

struct StructField
{
    std::string name;
    std::string data_type;
    std::string base_data_type;
    int rows = 1;
    int cols = 1;
    uint32_t offset = 0;
    uint32_t size=0;

    using sub_fields_type = boost::multi_index_container<StructField,boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<StructFieldByName>,
                boost::multi_index::member<StructField, std::string, &StructField::name>>,
            boost::multi_index::sequenced<
                boost::multi_index::tag<StructFieldByInsertionOrder>>
    >>;

    std::unique_ptr<sub_fields_type> sub_fields=std::make_unique<sub_fields_type>();


    StructField()=default;
    // Copy constructor: deep copy sub_fields
    StructField(const StructField& other)
        : name(other.name), data_type(other.data_type), base_data_type(other.base_data_type),
          rows(other.rows), cols(other.cols), offset(other.offset), size(other.size),
          sub_fields(other.sub_fields ? std::make_unique<sub_fields_type>(*other.sub_fields) : nullptr)
    {}

    // Copy assignment: deep copy sub_fields
    StructField& operator=(const StructField& other) {
        if (this != &other) {
            name = other.name;
            data_type = other.data_type;
            base_data_type = other.base_data_type;
            rows = other.rows;
            cols = other.cols;
            offset = other.offset;
            size = other.size;
            sub_fields = other.sub_fields ? std::make_unique<sub_fields_type>(*other.sub_fields) : nullptr;
        }
        return *this;
    }

    // Default move operations
    StructField(StructField&&) = default;
    StructField& operator=(StructField&&) = default;

    // Default destructor
    ~StructField() = default;

    const StructField* find(const std::string& parameter_name) const;
    const StructField* find_offset(const std::span<std::string>& path) const;

    static  StructField readStructField(const SpvReflectBlockVariable* block_description);
    std::string initDataC(void* data, const std::string& suffix="") const;

private:
    //recursive C-style struct init
    void initDataC(const std::string& prefix, std::stringstream& s, void* data, const std::string& suffix)const;
};

std::string primitive_to_str(const std::string& type, void* data);
void init_primitive(const std::string& prefix, const StructField& field, void* data, std::stringstream& s);
int primitive_size(const std::string& type);


inline void to_json(nlohmann::json& j, const StructField& field)
{
    j = nlohmann::json{
        {"name", field.name},
        {"data_type", field.data_type},
        {"base_data_type", field.base_data_type},
        {"rows", field.rows},
        {"cols", field.cols},
        {"offset", field.offset},
        {"size", field.size},
    };

    j["sub_fields"] = nlohmann::json::array();
    if (field.sub_fields)
    {
        for (const auto& sub_field : *field.sub_fields)
        {
            j["sub_fields"].push_back(sub_field);
        }
    }
}

inline void from_json(const nlohmann::json& j, StructField& field)
{
    field.name = j.value("name", std::string{});
    field.data_type = j.value("data_type", std::string{});
    field.base_data_type = j.value("base_data_type", std::string{});
    field.rows = j.value("rows", 1);
    field.cols = j.value("cols", 1);
    field.offset = j.value("offset", uint32_t{0});
    field.size = j.value("size", uint32_t{0});

    field.sub_fields = std::make_unique<StructField::sub_fields_type>();
    if (auto it = j.find("sub_fields"); it != j.end() && it->is_array())
    {
        for (const auto& sub_field_json : *it)
        {
            field.sub_fields->insert(sub_field_json.get<StructField>());
        }
    }
}

static inline bool HasFlag(const SpvReflectTypeDescription *td, SpvReflectTypeFlagBits flag)
{
    return td && ((td->type_flags & flag) != 0);
}
static std::string HlslScalarBase(const SpvReflectTypeDescription *td);
static std::string HlslNumericType(const SpvReflectTypeDescription *td);
// Returns the HLSL type string for a reflected variable, optionally with array suffixes.
// For structs with no type name, falls back to the variable name if available.
static std::string GetHlslTypeName(const SpvReflectTypeDescription *td, bool append_array_suffix = true);

void translateData(const StructField& src_struct,const StructField& dst_struct, void* src_data, void* dst_data);
static std::vector<char> translateData(const StructField& src_struct,const StructField& dst_struct, void* src_data);



