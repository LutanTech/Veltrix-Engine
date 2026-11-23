
#pragma once

#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Veltrix {

class FontManager {
public:
    bool initialize(const std::string& fontPath);
    void shutdown();

    FT_Face face() const;
    bool setSize(unsigned int size);

    float measureText(const std::string& text, unsigned int size);
    float lineHeight(unsigned int size);

private:
    FT_Library library = nullptr;
    FT_Face fontFace = nullptr;
};

}
