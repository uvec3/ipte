#include "StructField.hpp"
#include <boost/algorithm/string.hpp>

#include "Parameters.hpp"

const StructField* StructField::find(const std::string& parameter_name) const
{
    std::vector<std::string> path;
    boost::split(path, parameter_name, boost::is_any_of("."));
    return find_offset(path);
}

const StructField* StructField::find_offset(const std::span<std::string>& path) const
{
    auto& index=sub_fields->get<StructFieldByName>();

    const auto it=index.find(path[0]);
    if (it==index.end()&&sub_fields->size()==1)
    {
        return sub_fields->begin()->find_offset(path);
    }
    else
    {
        if (path.size()==1)
            return &(*it);
        return it->find_offset(std::span(path.begin()+1,path.end()));
    }
    return nullptr;
}


StructField StructField::readStructField(const SpvReflectBlockVariable* block_description)
{
    auto type_description = block_description->type_description;
    StructField field;
    field.name=type_description->struct_member_name?type_description->struct_member_name:block_description->name;
    field.cols = type_description->traits.numeric.matrix.column_count;
    field.rows = type_description->traits.numeric.matrix.row_count;
    field.data_type = GetHlslTypeName(type_description, false);
    field.base_data_type = HlslScalarBase(type_description);
    if (!field.cols)
        field.cols = type_description->traits.numeric.vector.component_count;
    if (field.cols == 0)
        field.cols = 1;

    if (field.rows==0 && type_description->traits.array.dims_count == 1)
    {
        field.rows = type_description->traits.array.dims[0];
    }
    if (field.rows == 0)
        field.rows = 1;

    if (field.cols >1&&field.rows==1)
        field.data_type+=std::to_string(field.cols);
    else if (field.rows >1&&field.cols >1)
        field.data_type+=std::to_string(field.rows)+"x"+std::to_string(field.cols);

    field.offset=block_description->offset;
    field.size=block_description->size;

    // if (block_description->member_count==1 && block_description->members->member_count!=0)
    //     block_description=block_description->members;
    for (int i=0;i<block_description->member_count;++i)
    {
        auto member=block_description->members+i;
        StructField sub_field=readStructField(member);
        field.sub_fields->insert(std::move(sub_field));
    }
    if (field.size==0)
    {
        auto& last_child=field.sub_fields->get<StructFieldByInsertionOrder>().back();
        field.size=last_child.offset+last_child.size;
    }


    return field;
}

std::string StructField::initDataC(void* data, const std::string& suffix) const
{
    std::stringstream s;
    initDataC("",s,data, suffix);
    return s.str();
}

void StructField::initDataC(const std::string& prefix, std::stringstream& s, void* data, const std::string& suffix) const
{
    std::string p=prefix+name+suffix;
    if (sub_fields->empty())
    {
        init_primitive(p,*this,data,s);
    }

    for (auto & f:*sub_fields)
    {
        f.initDataC(p+".",s,data, suffix);
    }
}

void primitive_to_str(std::ostream& s, const std::string& type, void* data)
 {
    if (type == "float")
    {
        const float value = *static_cast<float*>(data);
        if (std::isnan(value)) { s << "0.0/0.0"; return; }
        if (std::isinf(value)) { s << (value > 0 ? "1.0/0.0" : "-1.0/0.0"); return; }
        s  << value;
        return;
    }
    if (type == "int") { s << *static_cast<int*>(data); return; }
    if (type == "uint") { s << *static_cast<unsigned int*>(data); return; }
    if  (type == "bool") { s << (*static_cast<bool*>(data) ? "true" : "false"); return; }
    if (type == "double")
    {
        const double value = *static_cast<double*>(data);
        if (std::isnan(value)) { s << "0.0/0.0"; return; }
        if (std::isinf(value)) { s << (value > 0 ? "1.0/0.0" : "-1.0/0.0"); return; }
        s  << value;
        return;
    }
     if (type == "half")
     {
         // Convert half to float for printing
         uint16_t half_value = *static_cast<uint16_t*>(data);
         uint32_t float_value = (half_value & 0x8000) << 16 | // Sign bit
                               ((half_value & 0x7C00) + 0x1C000) << 13 | // Exponent
                               (half_value & 0x03FF) << 13; // Mantissa
         float result;
         std::memcpy(&result, &float_value, sizeof(float));
         if (std::isnan(result)) { s << "0.0/0.0"; return; }
         if (std::isinf(result)) { s << (result > 0 ? "1.0/0.0" : "-1.0/0.0"); return; }
         s << result;
         return;
     }
    if (type == "int64_t") { s << *static_cast<int64_t*>(data); return; }
    if (type == "uint64_t") { s << *static_cast<uint64_t*>(data); return; }
    if (type == "int_8") { s << static_cast<int>(*static_cast<int8_t*>(data)); return; }
    if (type == "uint_8") { s << static_cast<unsigned int>(*static_cast<uint8_t*>(data)); return; }
    if (type == "int_16") { s << *static_cast<int16_t*>(data); return; }
    if (type == "uint_16") { s << *static_cast<uint16_t*>(data); return; }
    s << "{}";
 }

void init_primitive(const std::string& prefix, const StructField& field, void* data, std::stringstream& s)
{
    s << std::setprecision(30);
    char* ptr = static_cast<char*>(data) + field.offset;
    int stride = primitive_size(field.base_data_type);
    for (int row = 0; row < field.rows; ++row)
        for (int col = 0; col < field.cols; ++col)
        {
            std::string element_prefix = prefix;
            if (field.rows > 1)
                element_prefix += "[" + std::to_string(row) + "]";
            if (field.cols > 1)
                element_prefix += "[" + std::to_string(col) + "]";
            s << element_prefix << " = ";
            primitive_to_str(s, field.base_data_type, ptr);
            s << ";\n";
            ptr += stride;
        }
}


std::string HlslScalarBase(const SpvReflectTypeDescription* td)
{
    if (!td) return "unknown";

    if (HasFlag(td, SPV_REFLECT_TYPE_FLAG_BOOL))
    {
        return "bool";
    }

    if (HasFlag(td, SPV_REFLECT_TYPE_FLAG_FLOAT))
    {
        const uint32_t w = td->traits.numeric.scalar.width;
        if (w == 16) return "half"; // or "min16float" if you prefer
        if (w == 32) return "float";
        if (w == 64) return "double";
        return "float";
    }

    if (HasFlag(td, SPV_REFLECT_TYPE_FLAG_INT))
    {
        const uint32_t w = td->traits.numeric.scalar.width;
        const bool s = td->traits.numeric.scalar.signedness != 0;
        // Map to common HLSL names. You can switch to min-precision types if desired.
        if (w == 8) return s ? "int8_t" : "uint8_t";
        if (w == 16) return s ? "int16_t" : "uint16_t";
        if (w == 32) return s ? "int" : "uint";
        if (w == 64) return s ? "int64_t" : "uint64_t";
        return s ? "int" : "uint";
    }

    if (HasFlag(td, SPV_REFLECT_TYPE_FLAG_STRUCT))
    {
        // Handled by caller for better fallback; return empty to signal struct.
        return "";
    }

    return "unknown";
}

std::string HlslNumericType(const SpvReflectTypeDescription* td)
{
    if (!td) return "unknown";

    // Structs: return type_name if available
    if (HasFlag(td, SPV_REFLECT_TYPE_FLAG_STRUCT))
    {
        if (td->type_name && td->type_name[0] != '\0')
        {
            return std::string(td->type_name);
        }
        // No type name available; caller can choose a fallback (e.g., variable name).
        return "UnknownType";
    }

    std::string base = HlslScalarBase(td);
    if (base.empty()) base = "unknown";

    // Scalar
    return base;
}

std::string GetHlslTypeName(const SpvReflectTypeDescription* td, bool append_array_suffix)
{
    std::string type_name;

    if (HasFlag(td, SPV_REFLECT_TYPE_FLAG_STRUCT))
    {
        if (td->type_name && td->type_name[0] != '\0')
        {
            type_name = td->type_name;

            if (type_name.ends_with("_std140"))
            {
                type_name = type_name.substr(0, type_name.size() - 7);
            }
            else if (type_name.ends_with("_std430"))
            {
                type_name = type_name.substr(0, type_name.size() - 7);
            }
            else if (type_name.ends_with("std140"))
            {
                type_name = type_name.substr(0, type_name.size() - 6);
            }
            else if (type_name.ends_with("std430"))
            {
                type_name = type_name.substr(0, type_name.size() - 6);
            }
        } else
        {
            type_name = "UnknownType";
        }
    } else
    {
        type_name = HlslNumericType(td);
    }

    return type_name;
}

void translateData(const StructField& src_struct, const StructField& dst_struct, void* src_data, void* dst_data)
{
    if (src_struct.sub_fields->empty()&&dst_struct.sub_fields->empty())
    {
        void* src_start=static_cast<char*>(src_data)+src_struct.offset;
        void* dst_start=static_cast<char*>(dst_data)+dst_struct.offset;
        if (src_struct.data_type==dst_struct.data_type && src_struct.size==dst_struct.size)
        {
            std::memcpy(dst_start,src_start,src_struct.size);
        }
        else
        {
            auto convertable_from=getConvertable(src_struct.data_type,src_start);
            auto convertable_to=getConvertable(dst_struct.data_type,dst_start);
            if (!convert(convertable_from,convertable_to))
            {
                if (src_struct.size==dst_struct.size)
                {
                    std::memcpy(src_start,dst_start,src_struct.size);
                }
                else
                {
                    std::memset(dst_start,0,dst_struct.size);
                }
            }
        }
    }
    else
    {
        for (const auto& dst_subfield : *dst_struct.sub_fields)
        {
            auto& src_subfields_index = src_struct.sub_fields->get<StructFieldByName>();
            auto it=src_subfields_index.find(dst_subfield.name);
            if (it!=src_subfields_index.end())
            {
                translateData(*it,dst_subfield,src_data,dst_data);
            }
        }
    }
}

int primitive_size(const std::string& type)
{
    if (type == "float") return 4;
    if (type == "int") return 4;
    if (type == "uint") return 4;
    if (type == "bool") return 4;
    if (type == "double") return 8;
    if (type == "half") return 2;
    if (type == "int64_t") return 8;
    if (type == "uint64_t") return 8;
    if (type == "int8_t") return 1;
    if (type == "uint8_t") return 1;
    if (type == "int16_t") return 2;
    if (type == "uint16_t") return 2;
    return 0; // Unknown type
}
