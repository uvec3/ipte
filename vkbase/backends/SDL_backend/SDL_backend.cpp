#include "../../core/SystemBeckend.hpp"
#include "../../core/Event.hpp"
#include "SDL_vulkan.h"
#include <vulkan/vulkan.h>
#include <SDL.h>



#if ANDROID
#include <android/log.h>
#define LOGI(...) \
        ((void)__android_log_print(ANDROID_LOG_INFO, "threaded_app", __VA_ARGS__))
#define LOGE(...) \
        ((void)__android_log_print(ANDROID_LOG_ERROR, "threaded_app", __VA_ARGS__))
#define LOGW(...) \
        ((void)__android_log_print(ANDROID_LOG_WARN, "threaded_app", __VA_ARGS__))
#else

#include <cstdio>
#include <stdexcept>

#define LOGI(...) \
        ((void)printf(__VA_ARGS__))
#define LOGE(...)   \
        ((void)fprintf(stderr, __VA_ARGS__))
#define LOGW(...)   \
        ((void)fprintf(stderr, __VA_ARGS__))
#endif

namespace vkbase::sys
{
    Event<SDL_Event*> sdlEvent;

    SDL_Window *window;
    int w, h;

    void fullscreen(bool enable)
    {
        if (enable)
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
        else
            SDL_SetWindowFullscreen(window, 0);
    }

    void init()
    {
        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
        {
            //printf("Error: %s\n", SDL_GetError());
            throw std::runtime_error("SDL init error: " + std::string(SDL_GetError()));
        }

        // get monitors count
        int monitorsCount = SDL_GetNumVideoDisplays();
        int display=0;

        //get the display mode
        SDL_DisplayMode DM;
        SDL_GetDisplayMode(display,0, &DM);
        int width = DM.w;
        int height = DM.h;


        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN
                                                         | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
        SDL_Window* window = SDL_CreateWindow("Fractal Engine", 0,0,
                                              width, height, window_flags);

        sys::window = window;


        //disable SDL mouse simulation with touch events
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    }

    void destroy()
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    std::vector<char> load_file(const char *path)
    {
        std::vector<char> buffer;
        SDL_RWops *rw = SDL_RWFromFile(path, "rb");
        if (rw != nullptr)
        {
            auto res_size = SDL_RWsize(rw);
            buffer.
                    resize(res_size);
            Sint64 nb_read_total = 0, nb_read = 1;
            char *buf = buffer.data();
            while (nb_read_total < res_size && nb_read != 0)
            {
                nb_read = SDL_RWread(rw, buf, 1, (res_size - nb_read_total));
                nb_read_total +=
                        nb_read;
                buf +=
                        nb_read;
            }
            SDL_RWclose(rw);
        }

        return
                buffer;
    }

    VkSurfaceKHR createSurface(VkInstance instance)
    {
        VkSurfaceKHR surface;
        if (
                SDL_Vulkan_CreateSurface(window, instance, &surface
                ) == 0)
            throw std::runtime_error("Failed to create surface");
        return
                surface;
    }

    //list of instance extensions which are necessary for platform
    std::vector<const char *> getRequiredInstanceExtensions()
    {
        std::vector<const char *> extensions;
        uint32_t extensions_count = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &extensions_count,
                                         nullptr);
        extensions.
                resize(extensions_count);
        SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions.

                        data()

        );
        return
                extensions;
    }

    int getWidth()
    {
        SDL_GetWindowSize(window, &w,
                          nullptr);
        return w;
    }

    int getHeight()
    {
        SDL_GetWindowSize(window,
                          nullptr, &h);
        return h;
    }

    float getDPI()
    {
        float dpi = 1.0f;
        int current_display = SDL_GetWindowDisplayIndex(window);
        if(!SDL_GetDisplayDPI(current_display, &dpi, nullptr, nullptr)||dpi<=0.0f)
            return 96.0f;
        return dpi;
    }

    void info(const char *str, int size)
    {
        LOGI("%.*s",size,str);
    }

    void error(const char *str, int size)
    {
        LOGE("%.*s",size,str);
    }

    void warning(const char *str, int size)
    {
        LOGW("%.*s",size,str);
    }

    bool handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                return false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                return false;


            switch (event.type)
            {
                //touch events
                case SDL_FINGERDOWN:
                {
                    engEvents.touchEvents.push(
                            {event.tfinger.fingerId, {event.tfinger.x * w, event.tfinger.y * h},
                             event.tfinger.timestamp,
                             TouchEvent::DOWN});
                    continue;
                }

                case SDL_FINGERUP:
                {
                    engEvents.touchEvents.push(
                            {event.tfinger.fingerId, {event.tfinger.x * w, event.tfinger.y * h},
                             event.tfinger.timestamp,
                             TouchEvent::UP});
                    continue;
                }

                case SDL_FINGERMOTION:
                {
                    engEvents.touchEvents.push(
                            {event.tfinger.fingerId, {event.tfinger.x * w, event.tfinger.y * h},
                             event.tfinger.timestamp,
                             TouchEvent::MOVE});
                    continue;
                }
            }

            sdlEvent.call(&event);
        }
        return true;
    }

    void captureKeyboard()
    {
        SDL_SetTextInputRect(nullptr);
        SDL_StartTextInput();
    }

    double getMilliseconds()
    {
        return static_cast<double>(SDL_GetTicks64());
    }

};