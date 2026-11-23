
#include "text_renderer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Veltrix {

namespace {

uint32_t nextCodepoint(const std::string& text, size_t& position) {
    unsigned char first = static_cast<unsigned char>(text[position++]);

    if (first < 0x80) {
        return first;
    }

    if ((first & 0xE0) == 0xC0 && position < text.size()) {
        return ((first & 0x1F) << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);
    }

    if ((first & 0xF0) == 0xE0 && position + 1 < text.size()) {
        uint32_t value = first & 0x0F;
        value = (value << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);
        value = (value << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);
        return value;
    }

    if ((first & 0xF8) == 0xF0 && position + 2 < text.size()) {
        uint32_t value = first & 0x07;

        for (int i = 0; i < 3; ++i) {
            value = (value << 6) |
                (static_cast<unsigned char>(text[position++]) & 0x3F);
        }

        return value;
    }

    return 0xFFFD;
}

}

bool TextRenderer::initialize(
    SDL_Renderer* renderer,
    const std::string& fontPath
) {
    this->renderer = renderer;

    if (!this->renderer) {
        return false;
    }

    if (!fontManager.initialize(fontPath)) {
        this->renderer = nullptr;
        return false;
    }

    return true;
}

void TextRenderer::shutdown() {
    fontManager.shutdown();
    renderer = nullptr;
}

float TextRenderer::measureText(
    const std::string& text,
    float size
) {
    return fontManager.measureText(
        text,
        static_cast<unsigned int>(std::max(1.0f, size))
    );
}

float TextRenderer::lineHeight(float size) {
    return fontManager.lineHeight(
        static_cast<unsigned int>(std::max(1.0f, size))
    );
}

void TextRenderer::render(const PaintText& text) {
    if (!renderer || !fontManager.face()) {
        return;
    }

    unsigned int size = static_cast<unsigned int>(
        std::max(1.0f, text.size)
    );

    if (!fontManager.setSize(size)) {
        return;
    }

    FT_Face face = fontManager.face();

    float cursorX = text.x;
    float baselineY = text.y;

    FT_UInt previousGlyph = 0;

    for (size_t position = 0; position < text.text.size();) {
        uint32_t codepoint = nextCodepoint(text.text, position);

        if (codepoint == '\r') {
            continue;
        }

        if (codepoint == '\n') {
            cursorX = text.x;
            baselineY += lineHeight(static_cast<float>(size));
            previousGlyph = 0;
            continue;
        }

        FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);

        if (FT_HAS_KERNING(face) && previousGlyph && glyphIndex) {
            FT_Vector kerning;

            if (FT_Get_Kerning(
                face,
                previousGlyph,
                glyphIndex,
                FT_KERNING_DEFAULT,
                &kerning
            ) == 0) {
                cursorX += static_cast<float>(kerning.x) / 64.0f;
            }
        }

        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT) != 0) {
            previousGlyph = glyphIndex;
            continue;
        }

        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            cursorX += static_cast<float>(
                face->glyph->advance.x
            ) / 64.0f;

            previousGlyph = glyphIndex;
            continue;
        }

        FT_GlyphSlot glyph = face->glyph;
        FT_Bitmap& bitmap = glyph->bitmap;

        if (bitmap.width > 0 && bitmap.rows > 0) {
            int width = static_cast<int>(bitmap.width);
            int height = static_cast<int>(bitmap.rows);

            std::vector<uint32_t> pixels(
                static_cast<size_t>(width) * height,
                0
            );

            for (int y = 0; y < height; ++y) {
                const unsigned char* source;

                if (bitmap.pitch >= 0) {
                    source = bitmap.buffer + y * bitmap.pitch;
                } else {
                    source = bitmap.buffer +
                        (height - 1 - y) * (-bitmap.pitch);
                }

                for (int x = 0; x < width; ++x) {
                    unsigned char alpha = 0;

                    if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                        alpha = source[x];

                        if (bitmap.num_grays > 1 &&
                            bitmap.num_grays != 256) {
                            alpha = static_cast<unsigned char>(
                                alpha * 255 / (bitmap.num_grays - 1)
                            );
                        }
                    } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                        alpha = (source[x / 8] &
                            (0x80 >> (x % 8))) ? 255 : 0;
                    }

                    pixels[
                        static_cast<size_t>(y) * width + x
                    ] =
                        (static_cast<uint32_t>(alpha) << 24) |
                        (static_cast<uint32_t>(text.color.b) << 16) |
                        (static_cast<uint32_t>(text.color.g) << 8) |
                        static_cast<uint32_t>(text.color.r);
                }
            }

            SDL_Texture* texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC,
                width,
                height
            );

            if (texture) {
                SDL_UpdateTexture(
                    texture,
                    nullptr,
                    pixels.data(),
                    width * sizeof(uint32_t)
                );

                SDL_SetTextureBlendMode(
                    texture,
                    SDL_BLENDMODE_BLEND
                );

                SDL_FRect destination = {
                    cursorX + static_cast<float>(glyph->bitmap_left),
                    baselineY - static_cast<float>(glyph->bitmap_top),
                    static_cast<float>(width),
                    static_cast<float>(height)
                };

                SDL_RenderTexture(
                    renderer,
                    texture,
                    nullptr,
                    &destination
                );

                SDL_DestroyTexture(texture);
            }
        }

        cursorX += static_cast<float>(
            glyph->advance.x
        ) / 64.0f;

        previousGlyph = glyphIndex;
    }
}

}
