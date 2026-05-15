#include "ShaderEditor.hpp"
#include "imgui_internal.h"

bool ShaderEditor::draw()
{
    TextEditor::ErrorMarkers errorMarkers;
    auto shader_name=currentShader->name;
    if (!currentShader->isCompiling())
    {
        if (currentShader->get_frag_result().diagnostics.contains(shader_name))
        {
            for (auto& [l,err] : currentShader->get_frag_result().diagnostics.at(shader_name).errorLines)
            {
                errorMarkers[l]=err.message;
            }
        }
    }
    mainEditor.SetErrorMarkers(errorMarkers);

    try
    {
        auto& io = ImGui::GetIO();
        mainEditor.Render("Code");
        //capture keyboard input if current window is focused
        if(ImGui::IsWindowFocused())
        {
            io.WantCaptureKeyboard = true;
        }

    }
    catch (const std::exception& e)
    {

    }


    if(mainEditor.IsTextChanged())
    {
        return true;
    }
    return false;
}


void ShaderEditor::setShader(ShaderModel *shader)
{
    currentShader = shader;
    initEditor(mainEditor, shader->language_name);
    setSourceCode(shader->getSource());
}



void ShaderEditor::initEditor(TextEditor& editor,const std::string& language)
{
    //Palette
    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Background] = ImGui::GetColorU32(ImGuiCol_WindowBg,0.5);
    palette[(int)TextEditor::PaletteIndex::CharLiteral] = ImGui::GetColorU32(ImGuiCol_Text,0.5);
    palette[(int)TextEditor::PaletteIndex::Comment] = 0x9900ff10;
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive]=ImGui::GetColorU32(ImGuiCol_WindowBg,0.7);

    editor.SetPalette(palette);

    if(language=="GLSL")
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    if(language=="HLSL")
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());

    editor.SetShowWhitespaces(false);
}


void ShaderEditor::setSourceCode(const std::string &code)
{
    auto cursorPos = mainEditor.GetCursorPosition();
    mainEditor.SetText(code);
    mainEditor.SetCursorPosition(cursorPos);
}

std::string ShaderEditor::getSourceCode()
{
    auto text=mainEditor.GetText();
    return text.substr(0,text.size()-1);
}
