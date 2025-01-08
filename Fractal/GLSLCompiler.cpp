#include "GLSLCompiler.h"
#include "../vkbase/extensions/ShadersRC/ShadersRC.hpp"

std::vector<uint32_t> GLSLCompiler::compile()
{
    return vkbase::ShadersRC::compileShader(getSource(), shaderName);
}

std::string GLSLCompiler::getSourceFromOther(AbstractShaderCompiler &other) {
    return "Translation from " + other.languageName + " to " + languageName + " is not supported";
}

GLSLCompiler::GLSLCompiler()
{
    languageName="GLSL";
}

std::vector<uint32_t> GLSLCompiler::compileCompute()
{
    return std::vector<uint32_t>();
}

std::vector<uint32_t> GLSLCompiler::compileForExport(std::string funcName, std::string additionalArguments, std::string parametersInit)
{
    return std::vector<uint32_t>();
}
