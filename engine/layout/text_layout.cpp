
#include "text_layout.h"
#include <algorithm>
#include <cctype>

namespace Veltrix {

bool TextLayout::initialize(const std::string& fontPath) {
    return fontManager.initialize(fontPath);
}

void TextLayout::shutdown() {
    fontManager.shutdown();
}

float TextLayout::measure(
    const std::string& text,
    float fontSize
) {
    return fontManager.measureText(
        text,
        static_cast<unsigned int>(std::max(1.0f, fontSize))
    );
}

float TextLayout::lineHeight(float fontSize) {
    return fontManager.lineHeight(
        static_cast<unsigned int>(std::max(1.0f, fontSize))
    );
}

size_t TextLayout::characterLength(
    const std::string& text,
    size_t position
) {
    unsigned char c = static_cast<unsigned char>(text[position]);

    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;

    return 1;
}

void TextLayout::appendWord(
    std::vector<TextLine>& lines,
    TextLine& current,
    const std::string& word,
    float availableWidth,
    float fontSize
) {
    if (word.empty()) return;

    float wordWidth = measure(word, fontSize);
    float spaceWidth = measure(" ", fontSize);

    if (!current.text.empty() &&
        current.width + spaceWidth + wordWidth <= availableWidth) {
        current.text += " " + word;
        current.width += spaceWidth + wordWidth;
        return;
    }

    if (!current.text.empty()) {
        lines.push_back(std::move(current));
        current = {};
    }

    if (wordWidth <= availableWidth || availableWidth <= 0) {
        current.text = word;
        current.width = wordWidth;
        return;
    }

    std::string fragment;

    for (size_t position = 0; position < word.size();) {
        size_t length = characterLength(word, position);
        length = std::min(length, word.size() - position);

        std::string character = word.substr(position, length);
        std::string candidate = fragment + character;

        if (!fragment.empty() &&
            measure(candidate, fontSize) > availableWidth) {
            current.text = fragment;
            current.width = measure(fragment, fontSize);

            lines.push_back(std::move(current));
            current = {};

            fragment = character;
        } else {
            fragment = std::move(candidate);
        }

        position += length;
    }

    if (!fragment.empty()) {
        current.text = fragment;
        current.width = measure(fragment, fontSize);
    }
}

std::vector<TextLine> TextLayout::wrap(
    const std::string& text,
    float availableWidth,
    float fontSize
) {
    std::vector<TextLine> lines;

    TextLine current;
    std::string word;

    for (size_t position = 0; position < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[position]);

        if (c == '\r' || c == '\n') {
            appendWord(
                lines,
                current,
                word,
                availableWidth,
                fontSize
            );

            word.clear();

            if (c == '\r' &&
                position + 1 < text.size() &&
                text[position + 1] == '\n') {
                ++position;
            }

            lines.push_back(std::move(current));
            current = {};

            ++position;
            continue;
        }

        if (std::isspace(c)) {
            appendWord(
                lines,
                current,
                word,
                availableWidth,
                fontSize
            );

            word.clear();
            ++position;
            continue;
        }

        size_t length = characterLength(text, position);
        length = std::min(length, text.size() - position);

        word += text.substr(position, length);
        position += length;
    }

    appendWord(
        lines,
        current,
        word,
        availableWidth,
        fontSize
    );

    if (!current.text.empty() || lines.empty()) {
        lines.push_back(std::move(current));
    }

    return lines;
}

}
