#include "font_manager.h"

namespace Veltrix {

bool FontManager::initialize(const std::string& fontPath) {
    if (FT_Init_FreeType(&library) != 0) {
        return false;
    }

    if (FT_New_Face(library, fontPath.c_str(), 0, &fontFace) != 0) {
        FT_Done_FreeType(library);
        library = nullptr;
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
    if (!fontFace) {
        return false;
    }

    return FT_Set_Pixel_Sizes(fontFace, 0, size) == 0;
}

}