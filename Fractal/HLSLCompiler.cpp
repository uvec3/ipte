#include "HLSLCompiler.h"

#include <shaderc/shaderc.hpp>
#include <iostream>
#include <fstream>

#include "../vkbase/extensions/Assets/assets.h"
#include "../vkbase/extensions/ShadersRC/slang/ShaderCompilerSlang.hpp"


HLSLCompiler hlsl_compiler;

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
        {"COMPUTE_SHADER", "1"},
};
std::map<std::string,std::string> fragmentShaderDefines= {};
std::map<std::string,std::string> exportDefines={};

using namespace std::literals::string_literals;


const std::string assets_prefix = "___assets___/"s;


vkbase::rc::FnLoadFile loadFN(const std::string& includePath, std::vector<std::string>* usedFiles_out)
{
    // when project isn't saved only use assets
    if (includePath.empty())
    {
        return[](const std::string& file) ->  std::optional<std::string>
        {
            std::string result_file;
            if (file.substr(0,assets_prefix.length())==assets_prefix)
                result_file= std::string(vkbase::assets::get("include/"+file.substr(assets_prefix.length())));
            else
                result_file= std::string(vkbase::assets::get("include/"+file));

            if (result_file.empty())
                return std::nullopt;
            return  result_file;
        };
    }

    //when project is saved, use the file system first then assets
    return[=](const std::string& file) -> std::optional<std::string>
    {
        //for files included with ""
        if (file.substr(0,assets_prefix.length())!=assets_prefix)
        {
            //first look in filesystem
            std::ifstream f(includePath+"/"+file);
            if (f)
            {
                if (usedFiles_out)
                    usedFiles_out->push_back(file);
                return std::string(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
            }
            //then look in assets
            else
            {
                auto asset_file=vkbase::assets::get("include/"+file);

                if (asset_file.empty())
                    return std::nullopt;
                else
                    return std::string(asset_file);
            }
        }
        //for files included with <>
        else
        {
            auto data= std::string(vkbase::assets::get("include/"+file.substr(assets_prefix.length())));
            if (!data.empty())
                return  data;
            return std::nullopt;
        }

    };
}

vkbase::rc::CompilationResult HLSLCompiler::compile(const std::string& src, const std::string& name, const std::string& includePath)
{
    vkbase::rc::CompilationParameters  info;
    info.compile_debug=true;
    info.defines=fragmentShaderDefines;
    info.entryPointName="main";
    info.fileName=name;
    info.source=src;
    info.type=vkbase::rc::ShaderType::Fragment;
    info.includePaths={assets_prefix};
    info.parse_diagnostics=true;
    info.loadFileFn=loadFN(includePath, nullptr);
    auto result=vkbase::rc::compileShaderSlang(info);

    result.spirv=std::move(result.spirv_debug);

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

vkbase::rc::CompilationResult HLSLCompiler::compileCompute(const std::string& src, const std::string& name, const std::string& includePath)
{
    auto code=src+"[numthreads(1, 1, 1)]void ___update___(){UpdateParameters();}";

    vkbase::rc::CompilationParameters  info;
    info.compile_debug=true;
    info.defines=computeShaderDefines;
    info.entryPointName="___update___";
    info.fileName=name;
    info.source=code;
    info.includePaths={assets_prefix};
    info.type= vkbase::rc::ShaderType::Compute;
    info.parse_diagnostics=true;
    info.loadFileFn=loadFN(includePath, nullptr);

    auto result=vkbase::rc::compileShaderSlang(info);
    result.spirv=std::move(result.spirv_debug);

    // vkbase::rc::SPIRVEditor spv_editor(result.spirv);
    //spv_editor.search_pattern({"*","=","Uniform"})

    return result;
}

vkbase::rc::CompilationResult HLSLCompiler::compileForExport(const std::string& src, const std::string& fileName,
        const std::string& funcName, const std::string& includePath)
{
    vkbase::rc::CompilationParameters info;
    info.compile_debug = true;
    info.defines={{"bool","int"}};
    info.entryPointName=funcName;
    info.fileName=fileName;
    info.source = src;
    info.includePaths={assets_prefix};
    info.type = vkbase::rc::ShaderType::Fragment;
    info.loadFileFn=loadFN(includePath, nullptr);
    info.target="GLSL";
    auto result= vkbase::rc::compileShaderSlang(info);

    return result;
}