#include "ShaderEditor.hpp"
#include "imgui_internal.h"

bool ShaderEditor::draw(const std::string& error)
{
    TextEditor::ErrorMarkers errorMarkers;

    //split by lines
    size_t begin=0;
    size_t end=0;
    while((end=error.find('\n',begin))!=std::string::npos)
    {
        size_t num_begin=error.find(':',begin)+1;
        size_t num_end=error.find(':',num_begin);

        auto lineNumStr=error.substr(num_begin, num_end-num_begin);
        try
        {
            int lineNum = std::stoi(lineNumStr);
            if(errorMarkers.contains(lineNum))
                errorMarkers[lineNum] +='\n';
            errorMarkers[lineNum] += error.substr(num_end + 1, end - num_end - 1);
        }
        catch (const std::exception&){}
        begin = end + 1;
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
    initEditor(mainEditor, shader->getCurrentCompiler());
}



void ShaderEditor::initEditor(TextEditor& editor, AbstractShaderCompiler& compiler)
{
    //Palette
    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Background] = ImGui::GetColorU32(ImGuiCol_WindowBg,0.5);
    palette[(int)TextEditor::PaletteIndex::CharLiteral] = ImGui::GetColorU32(ImGuiCol_Text,0.5);
    palette[(int)TextEditor::PaletteIndex::Comment] = 0x9900ff10;
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive]=ImGui::GetColorU32(ImGuiCol_WindowBg,0.7);

    editor.SetPalette(palette);

    if(compiler.languageName=="GLSL")
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    if(compiler.languageName=="HLSL")
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());

    editor.SetShowWhitespaces(false);
    editor.SetText(compiler.getSource());
}


void ShaderEditor::setSourceCode(const std::string &code)
{
    auto cursorPos = mainEditor.GetCursorPosition();
    mainEditor.SetText(code);
    mainEditor.SetCursorPosition(cursorPos);
}

std::string ShaderEditor::getSourceCode()
{
    return mainEditor.GetText();
}
