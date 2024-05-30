#pragma once
#include <memory>
#include "AbstractShaderCompiler.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"

class HLSLCompiler: public AbstractShaderCompiler
{
public:
    HLSLCompiler();
    std::vector<uint32_t> compile() override;

    std::vector<uint32_t> compileCompute() override;

    std::string getSourceFromOther(AbstractShaderCompiler &other) override;

    std::vector<uint32_t> compileForExport(std::string parametersInit) override;
};

