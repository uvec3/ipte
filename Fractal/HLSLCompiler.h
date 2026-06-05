#pragma once
#include <memory>
#include "AbstractShaderCompiler.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"


class HLSLCompiler: public AbstractShaderCompiler
{
public:
    HLSLCompiler();
    vkbase::rc::CompilationResult compile(const std::string& src, const std::string& name, const std::string& includePath) override;

    vkbase::rc::CompilationResult compileCompute(const std::string& src, const std::string& name, const std::string& includePath) override;

    std::string getSourceFromOther(AbstractShaderCompiler &other) override;

    vkbase::rc::CompilationResult compileForExport(const std::string& src, const std::string& fileName, const std::string& funcName, const std::string& includePath) override;
};

extern HLSLCompiler hlsl_compiler;

