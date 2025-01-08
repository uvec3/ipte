#include "SPIRVCompiler.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"

std::vector<uint32_t> SPIRVCompiler::compile()
{
    return vkbase::ShadersRC::compileShader(getSource(), shaderName+".spv");
}

std::string SPIRVCompiler::getSourceFromOther(AbstractShaderCompiler &other) {
    if(other.languageName=="GLSL")
        return vkbase::ShadersRC::compileShaderAsm(other.getSource(), other.shaderName);

    return "Translation from " + other.languageName + " to " + languageName + " is not supported";
}

SPIRVCompiler::SPIRVCompiler()
{
    languageName="SPIRV";
}

std::vector<uint32_t> SPIRVCompiler::compileCompute()
{
    return std::vector<uint32_t>();
}

std::vector<uint32_t> SPIRVCompiler::compileForExport(std::string funcName, std::string additionalArguments, std::string parametersInit)
{
    return std::vector<uint32_t>();
}
