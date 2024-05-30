#include "cppruntime.hpp"
#include <iostream>
#include <thread>
#include <chrono>

extern int glob;
int f()
{
    return 8;
}


int main()
{
    vkbase::cppruntime::Module module("test_func.c");
    int c=0;
    std::cout << "glob=" << glob << std::endl;
    while(true)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto f= reinterpret_cast<int (*)(int)>(module.getFunction("get33"));
        module.update();
        auto end = std::chrono::high_resolution_clock::now();

        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms res=" << (c=f(c)) << std::endl;

    }
}