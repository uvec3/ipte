#pragma once

namespace vkbase::cppruntime
{
    class Module
    {
    public:
        explicit Module(const char *filename);
        void *getFunction(const char *funcname);
        void update();
        ~Module();
    private:
        void *moduleState;
    };

    template<typename FuncType>
    class Function
    {
    public:
        Function(const char *filename, const char *funcname);
    };

}