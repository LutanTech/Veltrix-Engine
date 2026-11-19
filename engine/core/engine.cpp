#include "engine.h"

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
}

void Engine::run() {
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

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
                rect.y,
                rect.width,
                rect.height
            };

            SDL_RenderFillRect(renderer, &destination);
        }

        for (const auto& text : displayList.texts()) {
            textRenderer.render(text);
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