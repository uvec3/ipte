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

    virtual vkbase::rc::CompilationResult compile(const std::string& src, const std::string& name, const std::string& includePath) = 0;
    virtual vkbase::rc::CompilationResult compileCompute(const std::string& src, const std::string& name, const std::string& includePath) = 0;
    virtual vkbase::rc::CompilationResult compileForExport(const std::string& src, const std::string& name, const std::string& funcName, const std::string& includePath) = 0;
    virtual std::string getSourceFromOther(AbstractShaderCompiler& other) = 0;
    virtual ~AbstractShaderCompiler()= default;

};


