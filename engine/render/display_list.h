#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Veltrix {
struct Color {
    uint8_t r=0,g=0,b=0,a=255;
};
struct PaintRect {
    float x=0,y=0,width=0,height=0;
    Color color;
};
struct PaintText {
    float x=0,y=0,size=16;
    int weight=400;
    std::string text;
    Color color;
    bool underline=false;
};
enum class PaintElementType {
    HorizontalRule,
    Image,
    Input,
    Button,
    Link,
    Anchor
};
struct PaintElement {
    PaintElementType type=PaintElementType::HorizontalRule;
    float x=0,y=0,width=0,height=0;
    Color color;
    std::string source,alt,value,typeAttribute;
    Color textColor{35,35,35,255};
};
class DisplayList {
public:
    void addRect(float x,float y,float width,float height,Color color);
    void addText(float x,float y,float size,int weight,const std::string& text,Color color,bool underline=false);
    void addElement(PaintElementType type,float x,float y,float width,float height,Color color,const std::string& source="",const std::string& alt="",const std::string& value="",const std::string& typeAttribute="",Color textColor={35,35,35,255});
    const std::vector<PaintRect>& rectangles() const;
    const std::vector<PaintText>& texts() const;
    const std::vector<PaintElement>& elements() const;
private:
    std::vector<PaintRect> rects;
    std::vector<PaintText> textItems;
    std::vector<PaintElement> elementItems;
};
}