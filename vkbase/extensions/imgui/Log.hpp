#pragma once

#include <vector>
#include <string>
#include "imgui.hpp"
#include <mutex>

#include "../../core/glm/glm/glm.hpp"

namespace vkbase::imgui
{
    class Log
    {
    public:


        struct TextColor
        {
            uint64_t posStart;
            glm::vec4 color;
        };

        explicit Log(const char *name = "Log");

        std::vector<uint64_t> lines;
        std::vector<TextColor> colors;
        std::string content;
        std::mutex mutex;

        void clear();

        void draw();

        void add(const std::string_view &str, glm::vec4 color=glm::vec4(-1));

    private:
        bool show= true;
        std::string name = "Log";
    };


    void registerEngineMessagesLog();
    void unregisterEngineMessagesLog();
    void drawEngineMessagesLog();
}