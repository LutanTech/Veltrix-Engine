
#pragma once

#include <SDL3/SDL.h>
#include <string>

#include "font_manager.h"
#include "display_list.h"

namespace Veltrix {

class TextRenderer {
public:
    bool initialize(SDL_Renderer* renderer, const std::string& fontPath);
    void shutdown();

    void render(const PaintText& text);

    float measureText(const std::string& text, float size);
    float lineHeight(float size);

private:
    SDL_Renderer* renderer = nullptr;
    FontManager fontManager;
};

}
