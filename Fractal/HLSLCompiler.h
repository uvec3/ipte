#pragma once
#include <memory>
#include "AbstractShaderCompiler.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"


class HLSLCompiler: public AbstractShaderCompiler
{
public:
    HLSLCompiler();
    vkbase::ShadersRC::CompilationResult compile(const std::string& src, const std::string& name, const std::vector<std::string>& paths) override;

    vkbase::ShadersRC::CompilationResult compileCompute(const std::string& src, const std::string& name, const std::vector<std::string>& paths) override;

    std::string getSourceFromOther(AbstractShaderCompiler &other) override;

    vkbase::ShadersRC::CompilationResult compileForExport(const std::string& src, const std::string& name, const std::vector<std::string>& paths,std::string funcName, std::string additionalArguments,
                                                          std::string parametersInit) override;
};

