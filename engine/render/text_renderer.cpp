#include "text_renderer.h"

namespace Veltrix {

bool TextRenderer::initialize(SDL_Renderer* rendererPtr, const std::string& fontPath) {
    renderer = rendererPtr;
    return fontManager.initialize(fontPath);
}

void TextRenderer::shutdown() {
    fontManager.shutdown();
    renderer = nullptr;
}

void TextRenderer::render(const PaintText& text) {
    if (!renderer || !fontManager.face()) {
        return;
    }

    if (!fontManager.setSize(static_cast<unsigned int>(text.size))) {
        return;
    }

    FT_Face face = fontManager.face();

    float penX = text.x;
    float baseline = text.y;

    for (unsigned char character : text.text) {
        if (FT_Load_Char(face, character, FT_LOAD_RENDER) != 0) {
            continue;
        }

        FT_GlyphSlot glyph = face->glyph;

        if (glyph->bitmap.width == 0 || glyph->bitmap.rows == 0) {
            penX += glyph->advance.x / 64.0f;
            continue;
        }

        SDL_Surface* surface = SDL_CreateSurface(
            glyph->bitmap.width,
            glyph->bitmap.rows,
            SDL_PIXELFORMAT_RGBA32
        );

        if (!surface) {
            penX += glyph->advance.x / 64.0f;
            continue;
        }

        uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
        int pitch = surface->pitch;

        for (unsigned int y = 0; y < glyph->bitmap.rows; ++y) {
            for (unsigned int x = 0; x < glyph->bitmap.width; ++x) {
                uint8_t alpha = glyph->bitmap.buffer[
                    y * glyph->bitmap.pitch + x
                ];

                uint8_t* pixel = pixels + y * pitch + x * 4;

                pixel[0] = text.color.r;
                pixel[1] = text.color.g;
                pixel[2] = text.color.b;
                pixel[3] = alpha;
            }
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_DestroySurface(surface);

        if (!texture) {
            penX += glyph->advance.x / 64.0f;
            continue;
        }

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        SDL_FRect destination{
            penX + glyph->bitmap_left,
            baseline - glyph->bitmap_top,
            static_cast<float>(glyph->bitmap.width),
            static_cast<float>(glyph->bitmap.rows)
        };

        SDL_RenderTexture(
            renderer,
            texture,
            nullptr,
            &destination
        );

        SDL_DestroyTexture(texture);

        penX += glyph->advance.x / 64.0f;
    }
}

}