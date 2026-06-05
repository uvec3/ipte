#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "StructField.hpp"


inline bool edit_data_dynamic(const std::string& name, const std::string& type, int rows, int cols, void* data_ptr,
                              float stretch_width = 0,
                              const char* floatFormat = "%.9g")
{
    float speed = 0.1;
    ImGuiDataType dataType;
    int dataSize = 0;
    const void* max_ptr = nullptr;
    int stride;
    const char* format = nullptr;

    if (type == "float")
    {
        dataType = ImGuiDataType_Float;
        dataSize = sizeof(float);
        speed = 0.001;
        format = floatFormat;
        stride = sizeof(float);
    }
    else if (type == "double")
    {
        dataType = ImGuiDataType_Double;
        dataSize = sizeof(double);
        speed = 0.001;
        format = floatFormat;
        stride = sizeof(double);
    }
    else if (type == "int")
    {
        dataType = ImGuiDataType_S32;
        dataSize = sizeof(int32_t);
        stride = sizeof(int32_t);
    }
    else if (type == "uint")
    {
        dataType = ImGuiDataType_U32;
        dataSize = sizeof(uint32_t);
        stride = sizeof(uint32_t);
    }
    else if (type == "bool")
    {
        dataType = ImGuiDataType_U8;
        dataSize = sizeof(bool);
        static uint8_t bool_max = 1;
        max_ptr = &bool_max;
        stride = sizeof(bool);
    }
    else
    {
        ImGui::Text("Unsupported type %s", type.c_str());
        return false;
    }


    bool changed = false;

    int max_width = 0;
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            if (format == nullptr)
                format = ImGui::DataTypeGetInfo(dataType)->PrintFmt;
            void* data = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(data_ptr) + r * cols * dataSize + c *
                stride);
            char buffer[64];
            ImGui::DataTypeFormatString(buffer, 64, dataType, data, format);
            max_width = std::max<float>(ImGui::CalcTextSize(buffer).x, max_width);
        }
    }
    max_width += ImGui::GetStyle().FramePadding.x * 2;
    auto width = max_width * cols;
    if (stretch_width > 0)
        width = std::max<float>(width, stretch_width);


    ImGui::BeginGroup();
    for (int r = 0; r < rows; ++r)
    {
        ImGui::PushID(r);


        ImGui::SetNextItemWidth(width);
        changed |= ImGui::DragScalarN(std::format("##{}", name).c_str(), dataType,
                                      reinterpret_cast<void*>(
                                          reinterpret_cast<uintptr_t>(data_ptr) + r * cols * dataSize), cols,
                                      speed, nullptr, max_ptr, format);
        ImGui::NewLine();
        ImGui::PopID();
    }
    ImGui::EndGroup();

    return changed;
}


inline bool editStructFieldDynamic(const StructField& field, void* data_ptr,const std::function<void(std::function<void()>)>& draw_tooltip_callback, float available_width = 0,
                                   const char* float_format = "%f", bool no_collapse = false, bool header = false,
                                   bool default_open = true, float padding = 0, bool
                                   print_name = false)

{
    bool changed = false;
    void* data_with_offset = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data_ptr) + field.offset);
    if (field.sub_fields->size()!=0)
    {
        bool opened = true;
        if (header)
        {
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + padding -
                                       ImGui::CalcTextSize(field.name.c_str()).x - ImGui::GetFontSize() * 1.5 * (!
                                           no_collapse), ImGui::GetCursorPosY() + 7));
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanTextWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (default_open)
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            if (no_collapse)
            {
                ImGui::TextUnformatted(field.name.c_str());
            }
            else
            {
                opened = ImGui::TreeNodeEx(field.name.c_str(), flags);
            }

            if (ImGui::IsItemHovered())
            {
                draw_tooltip_callback( [_type=field.data_type, _offset=field.offset,_size=field.size]()
                {
                    ImGui::Text("Type: %s",_type.c_str());
                    ImGui::Text("Offset: %d",_offset);
                    ImGui::Text("Size: %d",_size);
                });
            }
        }

        if (opened)
        {
            float max_name_width = 0;
            if (field.sub_fields->size() > 1)
            {
                for (const auto& sub_field : field.sub_fields->get<StructFieldByInsertionOrder>())
                {
                    float name_width = ImGui::CalcTextSize(sub_field.name.c_str()).x;
                    if (!sub_field.sub_fields->empty() && !no_collapse)
                    {
                        name_width += ImGui::GetFontSize(); //account for tree arrow
                    }
                    max_name_width = std::max<float>(max_name_width, name_width);
                }
            }
            if (header)
                ImGui::SameLine();
            ImGui::BeginGroup();
            if (header)
                ImGui::NewLine();
            for (const auto& sub_field : field.sub_fields->get<StructFieldByInsertionOrder>())
            {
                changed |= editStructFieldDynamic(sub_field, data_with_offset,draw_tooltip_callback,
                                                  available_width - padding, float_format, no_collapse, true,
                                                  default_open, max_name_width,
                                                  field.sub_fields->size() > 1);
            }
            ImGui::EndGroup();
        }
    }
    else
    {
        if (padding)
        {
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + padding -
                                       ImGui::CalcTextSize(field.name.c_str()).x, ImGui::GetCursorPosY() + 7));
            ImGui::TextUnformatted(field.name.c_str());
            ImGui::SameLine();
        }
        if (ImGui::IsItemHovered())
        {
            draw_tooltip_callback( [_type=field.data_type, _offset=field.offset,_size=field.size]()
            {
                ImGui::Text("Type: %s",_type.c_str());
                ImGui::Text("Offset: %d",_offset);
                ImGui::Text("Size: %d",_size);
            });
        }

        changed |= edit_data_dynamic(field.name, field.base_data_type, field.rows, field.cols, data_with_offset,
                                     available_width - padding - 2, float_format);
    }

    return changed;
}
