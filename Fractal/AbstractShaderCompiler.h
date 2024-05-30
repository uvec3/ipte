#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>

class AbstractShaderCompiler
{
    std::string source;
    std::mutex sourceMutex;
public:

    std::string languageName;
    std::string shaderName="NewShader";

    virtual std::vector<uint32_t> compile() = 0;
    virtual std::vector<uint32_t> compileCompute() = 0;
    virtual std::vector<uint32_t> compileForExport(std::string parametersInit) = 0;
    virtual std::string getSourceFromOther(AbstractShaderCompiler& other) = 0;
    virtual ~AbstractShaderCompiler()= default;

    void setSource(const std::string &sourceCode)
    {
        sourceMutex.lock();
        source = sourceCode;
        sourceMutex.unlock();
    }

    std::string getSource()
    {
        sourceMutex.lock();
        std::string result = source;
        sourceMutex.unlock();
        return result;

    }
};


