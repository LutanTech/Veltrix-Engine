
#pragma once

#include <SDL3/SDL.h>
#include "render/display_list.h"
#include "render/text_renderer.h"

namespace Veltrix {

class Engine {
public:
    bool initialize();
    void setDisplayList(const DisplayList& list);
    void run();
    void shutdown();

private:
    bool running = false;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    DisplayList displayList;
    TextRenderer textRenderer;

    float scrollOffset = 0;
    float contentHeight = 0;
    float viewportHeight = 800;
    float scrollSpeed = 48;

    void updateContentHeight();
    void handleScroll(float amount);
};

}
