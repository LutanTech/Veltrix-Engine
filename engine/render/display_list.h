#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Veltrix {

struct Color {
    uint8_t r=0;
    uint8_t g=0;
    uint8_t b=0;
    uint8_t a=255;
};

struct PaintRect {
    float x=0;
    float y=0;
    float width=0;
    float height=0;
    Color color;
};

struct PaintText {
    float x=0;
    float y=0;
    float size=16;
    std::string text;
    Color color;
};

enum class PaintElementType {
    HorizontalRule,
    Image,
    Input,
    Button
};

struct PaintElement {
    PaintElementType type=PaintElementType::HorizontalRule;
    float x=0;
    float y=0;
    float width=0;
    float height=0;
    Color color;
    std::string source;
    std::string alt;
    std::string value;
    std::string typeAttribute;
};

class DisplayList {
public:
    void addRect(float x,float y,float width,float height,Color color);
    void addText(float x,float y,float size,const std::string& text,Color color);
    void addElement(PaintElementType type,float x,float y,float width,float height,Color color,const std::string& source="",const std::string& alt="",const std::string& value="",const std::string& typeAttribute="");

    const std::vector<PaintRect>& rectangles() const;
    const std::vector<PaintText>& texts() const;
    const std::vector<PaintElement>& elements() const;

private:
    std::vector<PaintRect> rects;
    std::vector<PaintText> textItems;
    std::vector<PaintElement> elementItems;
};

}