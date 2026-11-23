
#include "engine.h"
#include <algorithm>

namespace Veltrix {

bool Engine::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
    }

    window = SDL_CreateWindow(
        "Veltrix | Engineered to render.",
        1280,
        800,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    if (!textRenderer.initialize(
        renderer,
        "assets/fonts/Roboto-Regular.ttf"
    )) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    running = true;
    return true;
}

void Engine::setDisplayList(const DisplayList& list) {
    displayList = list;
    updateContentHeight();
    scrollOffset = 0;
}

void Engine::updateContentHeight() {
    contentHeight = 0;

    for (const auto& rect : displayList.rectangles()) {
        contentHeight = std::max(
            contentHeight,
            rect.y + rect.height
        );
    }

    for (const auto& text : displayList.texts()) {
        contentHeight = std::max(
            contentHeight,
            text.y + text.size
        );
    }

    int width = 0;
    int height = 0;

    if (window && SDL_GetWindowSize(window, &width, &height)) {
        viewportHeight = static_cast<float>(height);
    }

    scrollOffset = std::clamp(
        scrollOffset,
        0.0f,
        std::max(0.0f, contentHeight - viewportHeight)
    );
}

void Engine::handleScroll(float amount) {
    scrollOffset -= amount * scrollSpeed;

    scrollOffset = std::clamp(
        scrollOffset,
        0.0f,
        std::max(0.0f, contentHeight - viewportHeight)
    );
}

void Engine::run() {
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                float amount = event.wheel.y;

                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    amount = -amount;
                }

                handleScroll(amount);
            }
        }

        int windowWidth = 0;
        int windowHeight = 0;

        if (SDL_GetWindowSize(window, &windowWidth, &windowHeight)) {
            viewportHeight = static_cast<float>(windowHeight);
        }

        scrollOffset = std::clamp(
            scrollOffset,
            0.0f,
            std::max(0.0f, contentHeight - viewportHeight)
        );

        SDL_SetRenderDrawColor(renderer, 23, 17, 38, 255);
        SDL_RenderClear(renderer);

        for (const auto& rect : displayList.rectangles()) {
            SDL_SetRenderDrawColor(
                renderer,
                rect.color.r,
                rect.color.g,
                rect.color.b,
                rect.color.a
            );

            SDL_FRect destination{
                rect.x,
                rect.y - scrollOffset,
                rect.width,
                rect.height
            };

            SDL_RenderFillRect(renderer, &destination);
        }

        for (const auto& text : displayList.texts()) {
            PaintText visibleText = text;
            visibleText.y -= scrollOffset;

            textRenderer.render(visibleText);
        }

        SDL_RenderPresent(renderer);
    }
}

void Engine::shutdown() {
    textRenderer.shutdown();

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}

}
