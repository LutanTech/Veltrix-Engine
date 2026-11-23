
#pragma once

#include "../render/font_manager.h"
#include <string>
#include <vector>

namespace Veltrix {

struct TextLine {
    std::string text;
    float width = 0;
};

class TextLayout {
public:
    bool initialize(const std::string& fontPath);
    void shutdown();

    std::vector<TextLine> wrap(
        const std::string& text,
        float availableWidth,
        float fontSize
    );

    float measure(
        const std::string& text,
        float fontSize
    );

    float lineHeight(float fontSize);

private:
    FontManager fontManager;

    void appendWord(
        std::vector<TextLine>& lines,
        TextLine& current,
        const std::string& word,
        float availableWidth,
        float fontSize
    );

    static size_t characterLength(
        const std::string& text,
        size_t position
    );
};

}
