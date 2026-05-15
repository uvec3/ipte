#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"

class AbstractShaderCompiler
{
public:
    std::string languageName;

    virtual vkbase::ShadersRC::CompilationResult compile(const std::string& src, const std::string& name, const std::vector<std::string>& paths) = 0;
    virtual vkbase::ShadersRC::CompilationResult compileCompute(const std::string& src, const std::string& name, const std::vector<std::string>& paths) = 0;
    virtual vkbase::ShadersRC::CompilationResult compileForExport(const std::string& src, const std::string& name, const std::vector<std::string>& paths,std::string funcName, std::string additionalArguments,
                                                                  std::string parametersInit) = 0;
    virtual std::string getSourceFromOther(AbstractShaderCompiler& other) = 0;
    virtual ~AbstractShaderCompiler()= default;

};


