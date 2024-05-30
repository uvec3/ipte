#pragma once

#include "AbstractShaderCompiler.h"

class SPIRVCompiler: public AbstractShaderCompiler
{
public:
    SPIRVCompiler();
    std::vector<uint32_t> compile() override;
    std::string getSourceFromOther(AbstractShaderCompiler &other) override;

    std::vector<uint32_t> compileCompute() override;

    std::vector<uint32_t> compileForExport(std::string parametersInit) override;
};

