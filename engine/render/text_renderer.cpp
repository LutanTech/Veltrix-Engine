
#include "text_renderer.h"
#include FT_SYNTHESIS_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace Veltrix {

namespace {

uint32_t nextCodepoint(
    const std::string& text,
    size_t& position
) {
    unsigned char first =
        static_cast<unsigned char>(text[position++]);

    if (first < 0x80) {
        return first;
    }

    if ((first & 0xE0) == 0xC0 &&
        position < text.size()) {
        return ((first & 0x1F) << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);
    }

    if ((first & 0xF0) == 0xE0 &&
        position + 1 < text.size()) {
        uint32_t value = first & 0x0F;

        value = (value << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);

        value = (value << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);

        return value;
    }

    if ((first & 0xF8) == 0xF0 &&
        position + 2 < text.size()) {
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
    for (auto& entry : glyphCache) {
        if (entry.second.texture) {
            SDL_DestroyTexture(entry.second.texture);
        }
    }

    glyphCache.clear();

    fontManager.shutdown();
    renderer = nullptr;
}

float TextRenderer::measureText(
    const std::string& text,
    float size
) {
    return fontManager.measureText(
        text,
        static_cast<unsigned int>(
            std::max(1.0f, size)
        )
    );
}

float TextRenderer::lineHeight(float size) {
    return fontManager.lineHeight(
        static_cast<unsigned int>(
            std::max(1.0f, size)
        )
    );
}

float TextRenderer::baseline(float size) {
    unsigned int fontSize=static_cast<unsigned int>(std::max(1.0f,size));
    if(!fontManager.setSize(fontSize))return lineHeight(size)*.8f;
    FT_Face face=fontManager.face();
    if(!face||!face->size)return lineHeight(size)*.8f;
    float ascender=face->size->metrics.ascender/64.0f;
    float metricsHeight=face->size->metrics.height/64.0f;
    return (lineHeight(size)-metricsHeight)*.5f+ascender;
}

const TextRenderer::CachedGlyph* TextRenderer::getGlyph(
    uint32_t codepoint,
    unsigned int size,
    const PaintText& text
) {
    GlyphKey key{
        codepoint,
        size,
        text.color.r,
        text.color.g,
        text.color.b,
        text.color.a,
        static_cast<uint16_t>(text.weight)
    };

    auto found = glyphCache.find(key);

    if (found != glyphCache.end()) {
        return &found->second;
    }

    FT_Face face = fontManager.face();

    if (!face) {
        return nullptr;
    }

    CachedGlyph cached;

    cached.glyphIndex = FT_Get_Char_Index(face, codepoint);

    if (FT_Load_Glyph(
        face,
        cached.glyphIndex,
        FT_LOAD_DEFAULT
    ) != 0) {
        return nullptr;
    }

    if (text.weight >= 600)
        FT_GlyphSlot_Embolden(face->glyph);

    if (FT_Render_Glyph(
        face->glyph,
        FT_RENDER_MODE_NORMAL
    ) != 0) {
        return nullptr;
    }

    FT_GlyphSlot glyph = face->glyph;
    FT_Bitmap& bitmap = glyph->bitmap;

    cached.bitmapLeft = glyph->bitmap_left;
    cached.bitmapTop = glyph->bitmap_top;
    cached.advance = static_cast<float>(glyph->advance.x) / 64.0f;

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
                    alpha = (source[x / 8] & (0x80 >> (x % 8)))
                        ? 255 : 0;
                }

                alpha = static_cast<unsigned char>(
                    alpha * text.color.a / 255
                );

                pixels[
                    static_cast<size_t>(y) * width + x
                ] =
                    (static_cast<uint32_t>(alpha) << 24) |
                    (static_cast<uint32_t>(text.color.b) << 16) |
                    (static_cast<uint32_t>(text.color.g) << 8) |
                    static_cast<uint32_t>(text.color.r);
            }
        }

        cached.texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC,
            width,
            height
        );

        if (cached.texture) {
            SDL_UpdateTexture(
                cached.texture,
                nullptr,
                pixels.data(),
                width * sizeof(uint32_t)
            );

            SDL_SetTextureBlendMode(
                cached.texture,
                SDL_BLENDMODE_BLEND
            );
        }
    }

    auto result = glyphCache.emplace(key, cached);

    return &result.first->second;
}

void TextRenderer::render(const PaintText& text){renderRange(text,0,text.text.size());}

std::pair<float,float> TextRenderer::measureRangeBounds(const PaintText& text,size_t start,size_t end){
    if(!fontManager.face())return {0,0};
    unsigned int size=static_cast<unsigned int>(std::max(1.0f,text.size));
    if(!fontManager.setSize(size))return {0,0};
    FT_Face face=fontManager.face();
    float cursorX=0,left=std::numeric_limits<float>::max(),right=std::numeric_limits<float>::lowest();
    FT_UInt previousGlyph=0;
    for(size_t position=0;position<text.text.size();){
        size_t glyphStart=position;
        uint32_t codepoint=nextCodepoint(text.text,position);
        if(codepoint=='\r')continue;
        if(codepoint=='\n'){cursorX=0;previousGlyph=0;continue;}
        const CachedGlyph* glyph=getGlyph(codepoint,size,text);
        if(!glyph){previousGlyph=0;continue;}
        if(FT_HAS_KERNING(face)&&previousGlyph&&glyph->glyphIndex){
            FT_Vector kerning;
            if(FT_Get_Kerning(face,previousGlyph,glyph->glyphIndex,FT_KERNING_DEFAULT,&kerning)==0)cursorX+=static_cast<float>(kerning.x)/64.0f;
        }
        if(glyphStart>=start&&glyphStart<end){
            left=std::min(left,cursorX);
            right=std::max(right,cursorX+glyph->advance);
            if(glyph->texture){
                float width=0,height=0;
                SDL_GetTextureSize(glyph->texture,&width,&height);
                left=std::min(left,cursorX+glyph->bitmapLeft);
                right=std::max(right,cursorX+glyph->bitmapLeft+width);
            }
        }
        cursorX+=glyph->advance;
        previousGlyph=glyph->glyphIndex;
    }
    if(left==std::numeric_limits<float>::max())return {0,0};
    return {left,right};
}

void TextRenderer::renderRange(const PaintText& text,size_t start,size_t end){
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
        size_t glyphStart=position;
        uint32_t codepoint =
            nextCodepoint(text.text, position);

        if (codepoint == '\r') {
            continue;
        }

        if (codepoint == '\n') {
            cursorX = text.x;

            baselineY += lineHeight(
                static_cast<float>(size)
            );

            previousGlyph = 0;
            continue;
        }

        const CachedGlyph* glyph = getGlyph(
            codepoint,
            size,
            text
        );

        if (!glyph) {
            previousGlyph = 0;
            continue;
        }

        if (FT_HAS_KERNING(face) &&
            previousGlyph &&
            glyph->glyphIndex) {
            FT_Vector kerning;

            if (FT_Get_Kerning(
                face,
                previousGlyph,
                glyph->glyphIndex,
                FT_KERNING_DEFAULT,
                &kerning
            ) == 0) {
                cursorX +=
                    static_cast<float>(kerning.x) / 64.0f;
            }
        }

        if (glyphStart>=start&&glyphStart<end&&glyph->texture) {
            float width = 0;
            float height = 0;

            SDL_GetTextureSize(
                glyph->texture,
                &width,
                &height
            );

            SDL_FRect destination{
                cursorX + static_cast<float>(glyph->bitmapLeft),
                baselineY - static_cast<float>(glyph->bitmapTop),
                static_cast<float>(width),
                static_cast<float>(height)
            };

            SDL_RenderTexture(
                renderer,
                glyph->texture,
                nullptr,
                &destination
            );
        }

        cursorX += glyph->advance;
        previousGlyph = glyph->glyphIndex;
    }
}

}
