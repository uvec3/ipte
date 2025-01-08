#pragma once

#include "AbstractShaderCompiler.h"

class GLSLCompiler: public AbstractShaderCompiler
{
public:
    GLSLCompiler();
    std::vector<uint32_t> compile() override;
    std::string getSourceFromOther(AbstractShaderCompiler &other) override;

    std::vector<uint32_t> compileCompute() override;

    std::vector<uint32_t> compileForExport(std::string funcName, std::string additionalArguments, std::string parametersInit) override;


};

