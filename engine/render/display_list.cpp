#include "display_list.h"

namespace Veltrix {

void DisplayList::addRect(float x, float y, float width, float height, Color color) {
    rects.push_back({x, y, width, height, color});
}

void DisplayList::addText(float x, float y, float size, const std::string& text, Color color) {
    textItems.push_back({x, y, size, text, color});
}

const std::vector<PaintRect>& DisplayList::rectangles() const {
    return rects;
}

const std::vector<PaintText>& DisplayList::texts() const {
    return textItems;
}

}