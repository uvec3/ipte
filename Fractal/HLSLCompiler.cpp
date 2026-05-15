#include "HLSLCompiler.h"

#include <shaderc/shaderc.hpp>
#include <iostream>

#include "../vkbase/extensions/ShadersRC/slang/ShaderCompilerSlang.hpp"

//find "Type output(.....) {"
size_t findOutputFunction(const std::string& source,size_t& openingParenthesis, size_t& closingParenthesis,size_t& closingBrace)
{
    std::string outputFuncName{"output"};
    size_t begin=0;

    while(begin!=std::string::npos)
    {
        begin = source.find(outputFuncName, begin);

        //find '('
        auto i= openingParenthesis=source.find('(',begin);
        if(i==std::string::npos)
        {
            begin++;
            continue;
        }

        //check if 'output' is before '('
        bool checkFlag= true;
        for(size_t k=begin+ outputFuncName.length();k<i;k++)
        {
            if(!(source[k]==' '||source[k]=='\t'||source[k]=='\n'||source[k]=='\r'))
            {
                checkFlag= false;
                break;
            }
        }
        if(!checkFlag)
        {
            begin++;
            continue;
        }

        //find ')'
        closingParenthesis= i=source.find(')',i);
        if(i==std::string::npos)
        {
            begin++;
            continue;
        }

        //find '{'
        auto j=source.find('{',i);
        if(j==std::string::npos)
        {
            begin++;
            continue;
        }


        //check if '{' is after ')'
        checkFlag= true;
        for(size_t k=i+1;k<j;k++)
        {
            if(!(source[k]==' '||source[k]=='\t'||source[k]=='\n'||source[k]=='\r'))
            {
                checkFlag= false;
                break;
            }
        }
        if(checkFlag)
        {
            //find closing '}'
            closingBrace=j;
            int braceCount=1;
            for(closingBrace++;closingBrace<source.size();closingBrace++)
            {
                if(source[closingBrace]=='{')
                    braceCount++;
                else if(source[closingBrace]=='}')
                    braceCount--;
                if(braceCount==0)
                    break;
            }
            if(braceCount==0)
                return j+1;

            return std::string::npos;
        }
        begin++;
    }


    return std::string::npos;
}


std::map<std::string,std::string> computeShaderDefines= {
    {"InputParameters(TypeName, VarName)",
         "[[vk::binding(0,0)]] RWStructuredBuffer<TypeName> ___parameters___ : register(u0);"

         "static TypeName VarName =  ___parameters___[0];"

         "[[vk::binding(0,1)]] cbuffer k"
         "{"
         "  float4 keys[128];"
         "};"

         "bool isKeyPressed(int key)"
         "{"
         "  return keys[key/4][key%4]>1;"
         "}"

         "bool isKeyReleased(int key)"
         "{"
         "  return keys[key/4][key%4]<0;"
         "}"

         "bool isKeyDown(int key)"
         "{"
         "  return keys[key/4][key%4]>0;"
         "}" },
        {"KEY_None","0"},{"KEY_Tab","1"},{"KEY_LeftArrow","2"},{"KEY_RightArrow","3"},{"KEY_UpArrow","4"},{"KEY_DownArrow","5"},{"KEY_PageUp","6"},{"KEY_PageDown","7"},{"KEY_Home","8"},{"KEY_End","9"},{"KEY_Insert","10"},{"KEY_Delete","11"},{"KEY_Backspace","12"},{"KEY_Space","13"},{"KEY_Enter","14"},{"KEY_Escape","15"},{"KEY_LeftCtrl","16"},{"KEY_LeftShift","17"},{"KEY_LeftAlt","18"},{"KEY_LeftSuper","19"},{"KEY_RightCtrl","20"},{"KEY_RightShift","21"},{"KEY_RightAlt","22"},{"KEY_RightSuper","23"},{"KEY_Menu","24"},{"KEY_0","25"},{"KEY_1","26"},{"KEY_2","27"},{"KEY_3","28"},{"KEY_4","29"},{"KEY_5","30"},{"KEY_6","31"},{"KEY_7","32"},{"KEY_8","33"},{"KEY_9","34"},{"KEY_A","35"},{"KEY_B","36"},{"KEY_C","37"},{"KEY_D","38"},{"KEY_E","39"},{"KEY_F","40"},{"KEY_G","41"},{"KEY_H","42"},{"KEY_I","43"},{"KEY_J","44"},{"KEY_K","45"},{"KEY_L","46"},{"KEY_M","47"},{"KEY_N","48"},{"KEY_O","49"},{"KEY_P","50"},{"KEY_Q","51"},{"KEY_R","52"},{"KEY_S","53"},{"KEY_T","54"},{"KEY_U","55"},{"KEY_V","56"},{"KEY_W","57"},{"KEY_X","58"},{"KEY_Y","59"},{"KEY_Z","60"},{"KEY_F1","61"},{"KEY_F2","62"},{"KEY_F3","63"},{"KEY_F4","64"},{"KEY_F5","65"},{"KEY_F6","66"},{"KEY_F7","67"},{"KEY_F8","68"},{"KEY_F9","69"},{"KEY_F10","70"},{"KEY_F11","71"},{"KEY_F12","72"},{"KEY_Apostrophe","73"},{"KEY_Comma","74"},{"KEY_Minus","75"},{"KEY_Period","76"},{"KEY_Slash","77"},{"KEY_Semicolon","78"},{"KEY_Equal","79"},{"KEY_LeftBracket","80"},{"KEY_Backslash","81"},{"KEY_RightBracket","82"},{"KEY_GraveAccent","83"},{"KEY_CapsLock","84"},{"KEY_ScrollLock","85"},{"KEY_NumLock","86"},{"KEY_PrintScreen","87"},{"KEY_Pause","88"},{"KEY_Keypad0","89"},{"KEY_Keypad1","90"},{"KEY_Keypad2","91"},{"KEY_Keypad3","92"},{"KEY_Keypad4","93"},{"KEY_Keypad5","94"},{"KEY_Keypad6","95"},{"KEY_Keypad7","96"},{"KEY_Keypad8","97"},{"KEY_Keypad9","98"},{"KEY_KeypadDecimal","99"},{"KEY_KeypadDivide","100"},{"KEY_KeypadMultiply","101"},{"KEY_KeypadSubtract","102"},{"KEY_KeypadAdd","103"},{"KEY_KeypadEnter","104"},{"KEY_KeypadEqual","105"},{"KEY_GamepadStart","106"},{"KEY_GamepadBack","107"},{"KEY_GamepadFaceLeft","108"},{"KEY_GamepadFaceRight","109"},{"KEY_GamepadFaceUp","110"},{"KEY_GamepadFaceDown","111"},{"KEY_GamepadDpadLeft","112"},{"KEY_GamepadDpadRight","113"},{"KEY_GamepadDpadUp","114"},{"KEY_GamepadDpadDown","115"},{"KEY_GamepadL1","116"},{"KEY_GamepadR1","117"},{"KEY_GamepadL2","118"},{"KEY_GamepadR2","119"},{"KEY_GamepadL3","120"},{"KEY_GamepadR3","121"},{"KEY_GamepadLStickLeft","122"},{"KEY_GamepadLStickRight","123"},{"KEY_GamepadLStickUp","124"},{"KEY_GamepadLStickDown","125"},{"KEY_GamepadRStickLeft","126"},{"KEY_GamepadRStickRight","127"},{"KEY_GamepadRStickUp","128"},{"KEY_GamepadRStickDown","129"},{"KEY_MouseLeft","130"},{"KEY_MouseRight","131"},{"KEY_MouseMiddle","132"},{"KEY_MouseX1","133"},{"KEY_MouseX2","134"},{"KEY_MouseWheelX","135"},{"KEY_MouseWheelY","136"},{"KEY_Ctrl","137"},{"KEY_Shift","138"},{"KEY_Alt","139"},{"KEY_Super","140"},

        {"COMPUTE_SHADER", "1"},


};

std::map<std::string,std::string> fragmentShaderDefines= {

    {"InputParameters(TypeName, VarName)",
        "[[vk::binding(0, 0)]] layout(std140)   StructuredBuffer<TypeName> ___parameters___;"
        "static TypeName VarName =  ___parameters___[0];"

                                                 "[[vk::binding(0,1)]]  cbuffer k"
                                                 "{"
                                                 "  float4 keys[128];"
                                                 "};"

                                                 "bool isKeyPressed(int key)"
                                                 "{"
                                                 "  return keys[key/4][key%4]>1;"
                                                 "}"

                                                 "bool isKeyReleased(int key)"
                                                 "{"
                                                 "  return keys[key/4][key%4]<0;"
                                                 "}"

                                                 "bool isKeyDown(int key)"
                                                 "{"
                                                 "  return keys[key/4][key%4]>0;"
                                                 "}"
        },
        {"KEY_None","0"},{"KEY_Tab","1"},{"KEY_LeftArrow","2"},{"KEY_RightArrow","3"},{"KEY_UpArrow","4"},{"KEY_DownArrow","5"},{"KEY_PageUp","6"},{"KEY_PageDown","7"},{"KEY_Home","8"},{"KEY_End","9"},{"KEY_Insert","10"},{"KEY_Delete","11"},{"KEY_Backspace","12"},{"KEY_Space","13"},{"KEY_Enter","14"},{"KEY_Escape","15"},{"KEY_LeftCtrl","16"},{"KEY_LeftShift","17"},{"KEY_LeftAlt","18"},{"KEY_LeftSuper","19"},{"KEY_RightCtrl","20"},{"KEY_RightShift","21"},{"KEY_RightAlt","22"},{"KEY_RightSuper","23"},{"KEY_Menu","24"},{"KEY_0","25"},{"KEY_1","26"},{"KEY_2","27"},{"KEY_3","28"},{"KEY_4","29"},{"KEY_5","30"},{"KEY_6","31"},{"KEY_7","32"},{"KEY_8","33"},{"KEY_9","34"},{"KEY_A","35"},{"KEY_B","36"},{"KEY_C","37"},{"KEY_D","38"},{"KEY_E","39"},{"KEY_F","40"},{"KEY_G","41"},{"KEY_H","42"},{"KEY_I","43"},{"KEY_J","44"},{"KEY_K","45"},{"KEY_L","46"},{"KEY_M","47"},{"KEY_N","48"},{"KEY_O","49"},{"KEY_P","50"},{"KEY_Q","51"},{"KEY_R","52"},{"KEY_S","53"},{"KEY_T","54"},{"KEY_U","55"},{"KEY_V","56"},{"KEY_W","57"},{"KEY_X","58"},{"KEY_Y","59"},{"KEY_Z","60"},{"KEY_F1","61"},{"KEY_F2","62"},{"KEY_F3","63"},{"KEY_F4","64"},{"KEY_F5","65"},{"KEY_F6","66"},{"KEY_F7","67"},{"KEY_F8","68"},{"KEY_F9","69"},{"KEY_F10","70"},{"KEY_F11","71"},{"KEY_F12","72"},{"KEY_Apostrophe","73"},{"KEY_Comma","74"},{"KEY_Minus","75"},{"KEY_Period","76"},{"KEY_Slash","77"},{"KEY_Semicolon","78"},{"KEY_Equal","79"},{"KEY_LeftBracket","80"},{"KEY_Backslash","81"},{"KEY_RightBracket","82"},{"KEY_GraveAccent","83"},{"KEY_CapsLock","84"},{"KEY_ScrollLock","85"},{"KEY_NumLock","86"},{"KEY_PrintScreen","87"},{"KEY_Pause","88"},{"KEY_Keypad0","89"},{"KEY_Keypad1","90"},{"KEY_Keypad2","91"},{"KEY_Keypad3","92"},{"KEY_Keypad4","93"},{"KEY_Keypad5","94"},{"KEY_Keypad6","95"},{"KEY_Keypad7","96"},{"KEY_Keypad8","97"},{"KEY_Keypad9","98"},{"KEY_KeypadDecimal","99"},{"KEY_KeypadDivide","100"},{"KEY_KeypadMultiply","101"},{"KEY_KeypadSubtract","102"},{"KEY_KeypadAdd","103"},{"KEY_KeypadEnter","104"},{"KEY_KeypadEqual","105"},{"KEY_GamepadStart","106"},{"KEY_GamepadBack","107"},{"KEY_GamepadFaceLeft","108"},{"KEY_GamepadFaceRight","109"},{"KEY_GamepadFaceUp","110"},{"KEY_GamepadFaceDown","111"},{"KEY_GamepadDpadLeft","112"},{"KEY_GamepadDpadRight","113"},{"KEY_GamepadDpadUp","114"},{"KEY_GamepadDpadDown","115"},{"KEY_GamepadL1","116"},{"KEY_GamepadR1","117"},{"KEY_GamepadL2","118"},{"KEY_GamepadR2","119"},{"KEY_GamepadL3","120"},{"KEY_GamepadR3","121"},{"KEY_GamepadLStickLeft","122"},{"KEY_GamepadLStickRight","123"},{"KEY_GamepadLStickUp","124"},{"KEY_GamepadLStickDown","125"},{"KEY_GamepadRStickLeft","126"},{"KEY_GamepadRStickRight","127"},{"KEY_GamepadRStickUp","128"},{"KEY_GamepadRStickDown","129"},{"KEY_MouseLeft","130"},{"KEY_MouseRight","131"},{"KEY_MouseMiddle","132"},{"KEY_MouseX1","133"},{"KEY_MouseX2","134"},{"KEY_MouseWheelX","135"},{"KEY_MouseWheelY","136"},{"KEY_Ctrl","137"},{"KEY_Shift","138"},{"KEY_Alt","139"},{"KEY_Super","140"},
};

std::map<std::string,std::string> exportDefines={
        {"KEY_None","0"},{"KEY_Tab","1"},{"KEY_LeftArrow","2"},{"KEY_RightArrow","3"},{"KEY_UpArrow","4"},{"KEY_DownArrow","5"},{"KEY_PageUp","6"},{"KEY_PageDown","7"},{"KEY_Home","8"},{"KEY_End","9"},{"KEY_Insert","10"},{"KEY_Delete","11"},{"KEY_Backspace","12"},{"KEY_Space","13"},{"KEY_Enter","14"},{"KEY_Escape","15"},{"KEY_LeftCtrl","16"},{"KEY_LeftShift","17"},{"KEY_LeftAlt","18"},{"KEY_LeftSuper","19"},{"KEY_RightCtrl","20"},{"KEY_RightShift","21"},{"KEY_RightAlt","22"},{"KEY_RightSuper","23"},{"KEY_Menu","24"},{"KEY_0","25"},{"KEY_1","26"},{"KEY_2","27"},{"KEY_3","28"},{"KEY_4","29"},{"KEY_5","30"},{"KEY_6","31"},{"KEY_7","32"},{"KEY_8","33"},{"KEY_9","34"},{"KEY_A","35"},{"KEY_B","36"},{"KEY_C","37"},{"KEY_D","38"},{"KEY_E","39"},{"KEY_F","40"},{"KEY_G","41"},{"KEY_H","42"},{"KEY_I","43"},{"KEY_J","44"},{"KEY_K","45"},{"KEY_L","46"},{"KEY_M","47"},{"KEY_N","48"},{"KEY_O","49"},{"KEY_P","50"},{"KEY_Q","51"},{"KEY_R","52"},{"KEY_S","53"},{"KEY_T","54"},{"KEY_U","55"},{"KEY_V","56"},{"KEY_W","57"},{"KEY_X","58"},{"KEY_Y","59"},{"KEY_Z","60"},{"KEY_F1","61"},{"KEY_F2","62"},{"KEY_F3","63"},{"KEY_F4","64"},{"KEY_F5","65"},{"KEY_F6","66"},{"KEY_F7","67"},{"KEY_F8","68"},{"KEY_F9","69"},{"KEY_F10","70"},{"KEY_F11","71"},{"KEY_F12","72"},{"KEY_Apostrophe","73"},{"KEY_Comma","74"},{"KEY_Minus","75"},{"KEY_Period","76"},{"KEY_Slash","77"},{"KEY_Semicolon","78"},{"KEY_Equal","79"},{"KEY_LeftBracket","80"},{"KEY_Backslash","81"},{"KEY_RightBracket","82"},{"KEY_GraveAccent","83"},{"KEY_CapsLock","84"},{"KEY_ScrollLock","85"},{"KEY_NumLock","86"},{"KEY_PrintScreen","87"},{"KEY_Pause","88"},{"KEY_Keypad0","89"},{"KEY_Keypad1","90"},{"KEY_Keypad2","91"},{"KEY_Keypad3","92"},{"KEY_Keypad4","93"},{"KEY_Keypad5","94"},{"KEY_Keypad6","95"},{"KEY_Keypad7","96"},{"KEY_Keypad8","97"},{"KEY_Keypad9","98"},{"KEY_KeypadDecimal","99"},{"KEY_KeypadDivide","100"},{"KEY_KeypadMultiply","101"},{"KEY_KeypadSubtract","102"},{"KEY_KeypadAdd","103"},{"KEY_KeypadEnter","104"},{"KEY_KeypadEqual","105"},{"KEY_GamepadStart","106"},{"KEY_GamepadBack","107"},{"KEY_GamepadFaceLeft","108"},{"KEY_GamepadFaceRight","109"},{"KEY_GamepadFaceUp","110"},{"KEY_GamepadFaceDown","111"},{"KEY_GamepadDpadLeft","112"},{"KEY_GamepadDpadRight","113"},{"KEY_GamepadDpadUp","114"},{"KEY_GamepadDpadDown","115"},{"KEY_GamepadL1","116"},{"KEY_GamepadR1","117"},{"KEY_GamepadL2","118"},{"KEY_GamepadR2","119"},{"KEY_GamepadL3","120"},{"KEY_GamepadR3","121"},{"KEY_GamepadLStickLeft","122"},{"KEY_GamepadLStickRight","123"},{"KEY_GamepadLStickUp","124"},{"KEY_GamepadLStickDown","125"},{"KEY_GamepadRStickLeft","126"},{"KEY_GamepadRStickRight","127"},{"KEY_GamepadRStickUp","128"},{"KEY_GamepadRStickDown","129"},{"KEY_MouseLeft","130"},{"KEY_MouseRight","131"},{"KEY_MouseMiddle","132"},{"KEY_MouseX1","133"},{"KEY_MouseX2","134"},{"KEY_MouseWheelX","135"},{"KEY_MouseWheelY","136"},{"KEY_Ctrl","137"},{"KEY_Shift","138"},{"KEY_Alt","139"},{"KEY_Super","140"},

        {"InputParameters(Type, VarName)",
                "static Type VarName;"

                "bool isKeyPressed(int key)"
                "{"
                "  return false;"
                "}"

                "bool isKeyReleased(int key)"
                "{"
                "  return false;"
                "}"

                "bool isKeyDown(int key)"
                "{"
                "  return false;"
                "}"
        },
};


vkbase::ShadersRC::CompilationResult HLSLCompiler::compile(const std::string& src, const std::string& name, const std::vector<std::string>& paths)
{
    vkbase::ShadersRC::CompilationParameters  info;
    info.compile_debug=true;
    info.defines=fragmentShaderDefines;
    info.entryPointName="main";
    info.fileName=name;
    info.source=src;
    info.type=vkbase::ShadersRC::ShaderType::Fragment;
    info.includePaths=paths;
    info.parse_diagnostics=true;
    auto result=vkbase::ShadersRC::compileShaderSlang(info);

    auto optimized= std::move(result.spirv_debug);
    result.spirv=std::move(optimized);

    return result;
}

std::string HLSLCompiler::getSourceFromOther(AbstractShaderCompiler &other)
{
    return "Translation from " + other.languageName + " to " + languageName + " is not supported!";
}

HLSLCompiler::HLSLCompiler()
{
    languageName="HLSL";
}

vkbase::ShadersRC::CompilationResult HLSLCompiler::compileCompute(const std::string& src, const std::string& name, const std::vector<std::string>& paths)
{
    auto code=src+"[numthreads(1, 1, 1)]void ___update___(){UpdateParameters(___parameters___[0]);}";

    vkbase::ShadersRC::CompilationParameters  info;
    info.compile_debug=true;
    info.defines=computeShaderDefines;
    info.entryPointName="___update___";
    info.fileName=name;
    info.source=code;
    info.includePaths=paths;
    info.type= vkbase::ShadersRC::ShaderType::Compute;
    info.parse_diagnostics=true;

    auto result=vkbase::ShadersRC::compileShaderSlang(info);
    result.spirv=std::move(result.spirv_debug);
    return result;
}

vkbase::ShadersRC::CompilationResult HLSLCompiler::compileForExport(const std::string& src, const std::string& name, const std::vector<std::string>& paths,
                                                                    std::string funcName,
                                                                    std::string additionalArguments,
                                                                    std::string parametersInit)
{
    auto code=src;
    size_t openingParenthesis, closingParenthesis, closingBrace;
    size_t body = findOutputFunction(code, openingParenthesis, closingParenthesis,closingBrace);
    if(body==std::string::npos)
        throw std::runtime_error("Output function not found!");

    bool emptyParenthesis = true;
    for(size_t i=openingParenthesis+1;i<closingParenthesis;i++)
    {
        if(code[i]!=' '&&code[i]!='\t'&&code[i]!='\n'&&code[i]!='\r')
        {
            emptyParenthesis = false;
            break;
        }
    }
    if(!emptyParenthesis)
    {
        additionalArguments = "," + additionalArguments;
    }

    code=code.substr(0, closingBrace+1);
    code.insert(closingParenthesis, additionalArguments);
    code.insert(body+additionalArguments.size(), parametersInit);

    std::cout<<code<<std::endl;
    std::string diagnostics;


    vkbase::ShadersRC::CompilationParameters  info;
    info.compile_release=true;
    info.defines=exportDefines;
    info.entryPointName="output";
    info.fileName=name;
    info.source=code;
    info.includePaths=paths;
    info.type= vkbase::ShadersRC::ShaderType::Fragment;
    //info.parse_diagnostics=false;
    return vkbase::ShadersRC::compileShaderSlang(info);
}