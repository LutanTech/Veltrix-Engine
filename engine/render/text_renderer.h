
#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <cstdint>
#include <unordered_map>

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
    float baseline(float size);

private:
    struct GlyphKey {
        uint32_t codepoint = 0;
        unsigned int size = 0;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;
        uint16_t weight = 400;

        bool operator==(const GlyphKey& other) const {
            return codepoint == other.codepoint &&
                   size == other.size &&
                   r == other.r &&
                   g == other.g &&
                   b == other.b &&
                   a == other.a &&
                   weight == other.weight;
        }
    };

    struct GlyphKeyHash {
        size_t operator()(const GlyphKey& key) const {
            size_t hash = key.codepoint;
            hash = hash * 31 + key.size;
            hash = hash * 31 + key.r;
            hash = hash * 31 + key.g;
            hash = hash * 31 + key.b;
            hash = hash * 31 + key.a;
            hash = hash * 31 + key.weight;
            return hash;
        }
    };

    struct CachedGlyph {
        SDL_Texture* texture = nullptr;
        int bitmapLeft = 0;
        int bitmapTop = 0;
        float advance = 0;
        FT_UInt glyphIndex = 0;
    };

    SDL_Renderer* renderer = nullptr;
    FontManager fontManager;

    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> glyphCache;

    const CachedGlyph* getGlyph(
        uint32_t codepoint,
        unsigned int size,
        const PaintText& text
    );
};

}
