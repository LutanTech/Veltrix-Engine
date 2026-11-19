#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Veltrix {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

struct PaintRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    Color color;
};

struct PaintText {
    float x = 0;
    float y = 0;
    float size = 16;
    std::string text;
    Color color;
};

class DisplayList {
public:
    void addRect(float x, float y, float width, float height, Color color);
    void addText(float x, float y, float size, const std::string& text, Color color);

    const std::vector<PaintRect>& rectangles() const;
    const std::vector<PaintText>& texts() const;

private:
    std::vector<PaintRect> rects;
    std::vector<PaintText> textItems;
};

}