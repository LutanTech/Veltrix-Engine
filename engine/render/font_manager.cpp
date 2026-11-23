
#include "font_manager.h"
#include <cstdint>

namespace Veltrix {

namespace {

uint32_t nextCodepoint(const std::string& text, size_t& position) {
    unsigned char first = static_cast<unsigned char>(text[position++]);

    if (first < 0x80) {
        return first;
    }

    if ((first & 0xE0) == 0xC0 && position < text.size()) {
        uint32_t value = first & 0x1F;
        value = (value << 6) |
            (static_cast<unsigned char>(text[position++]) & 0x3F);
        return value;
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

bool FontManager::initialize(const std::string& fontPath) {
    shutdown();

    if (FT_Init_FreeType(&library) != 0) {
        library = nullptr;
        return false;
    }

    if (FT_New_Face(library, fontPath.c_str(), 0, &fontFace) != 0) {
        shutdown();
        return false;
    }

    return true;
}

void FontManager::shutdown() {
    if (fontFace) {
        FT_Done_Face(fontFace);
        fontFace = nullptr;
    }

    if (library) {
        FT_Done_FreeType(library);
        library = nullptr;
    }
}

FT_Face FontManager::face() const {
    return fontFace;
}

bool FontManager::setSize(unsigned int size) {
    if (!fontFace || size == 0) {
        return false;
    }

    return FT_Set_Pixel_Sizes(fontFace, 0, size) == 0;
}

float FontManager::measureText(
    const std::string& text,
    unsigned int size
) {
    if (!fontFace || !setSize(size)) {
        return 0;
    }

    float width = 0;
    FT_UInt previousGlyph = 0;

    for (size_t position = 0; position < text.size();) {
        uint32_t codepoint = nextCodepoint(text, position);

        if (codepoint == '\n' || codepoint == '\r') {
            break;
        }

        FT_UInt glyphIndex = FT_Get_Char_Index(fontFace, codepoint);

        if (FT_HAS_KERNING(fontFace) && previousGlyph && glyphIndex) {
            FT_Vector kerning;

            if (FT_Get_Kerning(
                fontFace,
                previousGlyph,
                glyphIndex,
                FT_KERNING_DEFAULT,
                &kerning
            ) == 0) {
                width += static_cast<float>(kerning.x) / 64.0f;
            }
        }

        if (FT_Load_Glyph(fontFace, glyphIndex, FT_LOAD_DEFAULT) == 0) {
            width += static_cast<float>(
                fontFace->glyph->advance.x
            ) / 64.0f;
        }

        previousGlyph = glyphIndex;
    }

    return width;
}

float FontManager::lineHeight(unsigned int size) {
    if (!fontFace || !setSize(size)) {
        return static_cast<float>(size);
    }

    return static_cast<float>(
        fontFace->size->metrics.height
    ) / 64.0f;
}

}
