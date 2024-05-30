#pragma once
#include "../vkbase/extensions/imgui/ImGuiColorTextEdit/TextEditor.h"
#include "../vkbase/extensions/imgui/imgui.hpp"
#include "../vkbase/extensions/imgui/Log.hpp"
#include "ShaderModel.hpp"


class ShaderEditor
{

public:
    vkbase::imgui::Log log{"Shader compilation log"};
    bool draw(const std::string& error);
    void setShader(ShaderModel *shader);
    void setSourceCode(const std::string &code);
    std::string getSourceCode();

private:
    ShaderModel *currentShader= nullptr;
    TextEditor mainEditor;
    std::vector<TextEditor> editors;

    static void initEditor(TextEditor& editor, AbstractShaderCompiler& compiler);
};
