#include "display_list_builder.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
namespace Veltrix {
namespace {
std::string trim(const std::string& value) {
 size_t start = value.find_first_not_of(" \t\n\r");
 if (start == std::string::npos) return "";
 size_t end = value.find_last_not_of(" \t\n\r");
 return value.substr(start, end - start + 1);
}
double parseNumber(const std::string& value, double fallback = 0) {
 char* end = nullptr;
 double result = std::strtod(value.c_str(), &end);
 if (end == value.c_str()) return fallback;
 return result;
}

std::vector<std::string> splitBorderTokens(const std::string& value){
    std::vector<std::string> tokens;
    std::string token;
    int depth=0;
    for(char c:value){
        if(std::isspace(static_cast<unsigned char>(c))&&depth==0){
            if(!token.empty()){
                tokens.push_back(token);
                token.clear();
            }
        }else{
            token+=c;
            if(c=='(')depth++;
            else if(c==')'&&depth>0)depth--;
        }
    }
    if(!token.empty())tokens.push_back(token);
    return tokens;
}

std::string borderBoxValue(const std::string& value,int side){
    auto values=splitBorderTokens(value);
    if(values.empty())return "";
    if(values.size()==1)return values[0];
    if(values.size()==2)return values[side==0||side==2?0:1];
    if(values.size()==3)return values[side==0?0:side==2?2:1];
    return values[side];
}

std::string shorthandBorderStyle(const std::string& border){
    for(const auto& token:splitBorderTokens(border)){
        if(token=="none"||token=="hidden"||token=="dotted"||token=="dashed"||token=="solid"||token=="double"||token=="groove"||token=="ridge"||token=="inset"||token=="outset")return token;
    }
    return "";
}

std::string shorthandBorderColor(const std::string& border){
    for(const auto& token:splitBorderTokens(border)){
        if(token=="none"||token=="hidden"||token=="dotted"||token=="dashed"||token=="solid"||token=="double"||token=="groove"||token=="ridge"||token=="inset"||token=="outset")continue;
        if(token=="thin"||token=="medium"||token=="thick")continue;
        if(!token.empty()&&(std::isdigit(static_cast<unsigned char>(token[0]))||token[0]=='.'))continue;
        return token;
    }
    return "";
}

void addBorderSide(DisplayList& displayList,float x,float y,float length,float thickness,bool horizontal,const Color& color,const std::string& style){
    if(length<=0||thickness<=0||style=="none"||style=="hidden")return;
    auto draw=[&](float offset,float segment,float band){
        if(segment<=0||band<=0)return;
        if(horizontal)displayList.addRect(x+offset,y,segment,band,color);
        else displayList.addRect(x,y+offset,band,segment,color);
    };
    if(style=="double"&&thickness>=3){
        float band=std::max(1.0f,thickness/3.0f);
        draw(0,length,band);
        if(horizontal)displayList.addRect(x,y+thickness-band,length,band,color);
        else displayList.addRect(x+thickness-band,y,band,length,color);
        return;
    }
    if(style=="dashed"||style=="dotted"){
        float segment=style=="dotted"?thickness:thickness*3.0f;
        float gap=thickness*2.0f;
        for(float offset=0;offset<length;offset+=segment+gap)
            draw(offset,std::min(segment,length-offset),thickness);
        return;
    }
    draw(0,length,thickness);
}

void addBorders(DisplayList& displayList,float x,float y,float width,float height,
                float top,float right,float bottom,float left,
                const Color& topColor,const Color& rightColor,const Color& bottomColor,const Color& leftColor,
                const std::string& topStyle,const std::string& rightStyle,const std::string& bottomStyle,const std::string& leftStyle){
    top=std::clamp(top,0.0f,std::max(0.0f,height));
    bottom=std::clamp(bottom,0.0f,std::max(0.0f,height-top));
    left=std::clamp(left,0.0f,std::max(0.0f,width));
    right=std::clamp(right,0.0f,std::max(0.0f,width-left));
    addBorderSide(displayList,x,y,width,top,true,topColor,topStyle);
    addBorderSide(displayList,x+width-right,y+top,std::max(0.0f,height-top-bottom),right,false,rightColor,rightStyle);
    addBorderSide(displayList,x,y+height-bottom,width,bottom,true,bottomColor,bottomStyle);
    addBorderSide(displayList,x,y+top,std::max(0.0f,height-top-bottom),left,false,leftColor,leftStyle);
}

double clamp(double value, double minimum, double maximum) {
 return std::max(minimum, std::min(value, maximum));
}
uint8_t channel(const std::string& value) {
 double number = parseNumber(value);
 if (value.find('%') != std::string::npos) {
 number = number * 2.55;
 }
 return static_cast<uint8_t>(clamp(std::round(number), 0, 255));
}
uint8_t alphaChannel(const std::string& value) {
 double number = parseNumber(value, 1);
 if (value.find('%') != std::string::npos) {
 number /= 100.0;
 }
 return static_cast<uint8_t>(
 std::round(clamp(number, 0, 1) * 255)
 );
}
std::vector<std::string> splitColorArguments(std::string value) {
 std::vector<std::string> result;
 for (char& c : value) {
 if (c == ',' || c == '/' || c == '\t' || c == '\n') {
 c = ' ';
 }
 }
 size_t position = 0;
 while (position < value.size()) {
 while (position < value.size() && value[position] == ' ') {
 ++position;
 }
 if (position >= value.size()) break;
 size_t end = value.find(' ', position);
 if (end == std::string::npos) {
 end = value.size();
 }
 result.push_back(value.substr(position, end - position));
 position = end;
 }
 return result;
}
Color hslToRgb(double h, double s, double l, double a) {
 h = std::fmod(h, 360.0);
 if (h < 0) h += 360.0;
 s = clamp(s, 0, 100) / 100.0;
 l = clamp(l, 0, 100) / 100.0;
 double c = (1 - std::abs(2 * l - 1)) * s;
 double x = c * (1 - std::abs(std::fmod(h / 60.0, 2) - 1));
 double m = l - c / 2;
 double r = 0;
 double g = 0;
 double b = 0;
 if (h < 60) {
 r = c; g = x;
 } else if (h < 120) {
 r = x; g = c;
 } else if (h < 180) {
 g = c; b = x;
 } else if (h < 240) {
 g = x; b = c;
 } else if (h < 300) {
 r = x; b = c;
 } else {
 r = c; b = x;
 }
 return {
 static_cast<uint8_t>(std::round((r + m) * 255)),
 static_cast<uint8_t>(std::round((g + m) * 255)),
 static_cast<uint8_t>(std::round((b + m) * 255)),
 static_cast<uint8_t>(std::round(clamp(a, 0, 1) * 255))
 };
}
const std::unordered_map<std::string, uint32_t>& namedColors() {
 static const std::unordered_map<std::string, uint32_t> colors = {
 {"aliceblue", 0xF0F8FF}, {"antiquewhite", 0xFAEBD7},
 {"aqua", 0x00FFFF}, {"aquamarine", 0x7FFFD4},
 {"azure", 0xF0FFFF}, {"beige", 0xF5F5DC},
 {"bisque", 0xFFE4C4}, {"black", 0x000000},
 {"blanchedalmond", 0xFFEBCD}, {"blue", 0x0000FF},
 {"blueviolet", 0x8A2BE2}, {"brown", 0xA52A2A},
 {"burlywood", 0xDEB887}, {"cadetblue", 0x5F9EA0},
 {"chartreuse", 0x7FFF00}, {"chocolate", 0xD2691E},
 {"coral", 0xFF7F50}, {"cornflowerblue", 0x6495ED},
 {"cornsilk", 0xFFF8DC}, {"crimson", 0xDC143C},
 {"cyan", 0x00FFFF}, {"darkblue", 0x00008B},
 {"darkcyan", 0x008B8B}, {"darkgoldenrod", 0xB8860B},
 {"darkgray", 0xA9A9A9}, {"darkgreen", 0x006400},
 {"darkgrey", 0xA9A9A9}, {"darkkhaki", 0xBDB76B},
 {"darkmagenta", 0x8B008B}, {"darkolivegreen", 0x556B2F},
 {"darkorange", 0xFF8C00}, {"darkorchid", 0x9932CC},
 {"darkred", 0x8B0000}, {"darksalmon", 0xE9967A},
 {"darkseagreen", 0x8FBC8F}, {"darkslateblue", 0x483D8B},
 {"darkslategray", 0x2F4F4F}, {"darkslategrey", 0x2F4F4F},
 {"darkturquoise", 0x00CED1}, {"darkviolet", 0x9400D3},
 {"deeppink", 0xFF1493}, {"deepskyblue", 0x00BFFF},
 {"dimgray", 0x696969}, {"dimgrey", 0x696969},
 {"dodgerblue", 0x1E90FF}, {"firebrick", 0xB22222},
 {"floralwhite", 0xFFFAF0}, {"forestgreen", 0x228B22},
 {"fuchsia", 0xFF00FF}, {"gainsboro", 0xDCDCDC},
 {"ghostwhite", 0xF8F8FF}, {"gold", 0xFFD700},
 {"goldenrod", 0xDAA520}, {"gray", 0x808080},
 {"green", 0x008000}, {"greenyellow", 0xADFF2F},
 {"grey", 0x808080}, {"honeydew", 0xF0FFF0},
 {"hotpink", 0xFF69B4}, {"indianred", 0xCD5C5C},
 {"indigo", 0x4B0082}, {"ivory", 0xFFFFF0},
 {"khaki", 0xF0E68C}, {"lavender", 0xE6E6FA},
 {"lavenderblush", 0xFFF0F5}, {"lawngreen", 0x7CFC00},
 {"lemonchiffon", 0xFFFACD}, {"lightblue", 0xADD8E6},
 {"lightcoral", 0xF08080}, {"lightcyan", 0xE0FFFF},
 {"lightgoldenrodyellow", 0xFAFAD2}, {"lightgray", 0xD3D3D3},
 {"lightgreen", 0x90EE90}, {"lightgrey", 0xD3D3D3},
 {"lightpink", 0xFFB6C1}, {"lightsalmon", 0xFFA07A},
 {"lightseagreen", 0x20B2AA}, {"lightskyblue", 0x87CEFA},
 {"lightslategray", 0x778899}, {"lightslategrey", 0x778899},
 {"lightsteelblue", 0xB0C4DE}, {"lightyellow", 0xFFFFE0},
 {"lime", 0x00FF00}, {"limegreen", 0x32CD32},
 {"linen", 0xFAF0E6}, {"magenta", 0xFF00FF},
 {"maroon", 0x800000}, {"mediumaquamarine", 0x66CDAA},
 {"mediumblue", 0x0000CD}, {"mediumorchid", 0xBA55D3},
 {"mediumpurple", 0x9370DB}, {"mediumseagreen", 0x3CB371},
 {"mediumslateblue", 0x7B68EE}, {"mediumspringgreen", 0x00FA9A},
 {"mediumturquoise", 0x48D1CC}, {"mediumvioletred", 0xC71585},
 {"midnightblue", 0x191970}, {"mintcream", 0xF5FFFA},
 {"mistyrose", 0xFFE4E1}, {"moccasin", 0xFFE4B5},
 {"navajowhite", 0xFFDEAD}, {"navy", 0x000080},
 {"oldlace", 0xFDF5E6}, {"olive", 0x808000},
 {"olivedrab", 0x6B8E23}, {"orange", 0xFFA500},
 {"orangered", 0xFF4500}, {"orchid", 0xDA70D6},
 {"palegoldenrod", 0xEEE8AA}, {"palegreen", 0x98FB98},
 {"paleturquoise", 0xAFEEEE}, {"palevioletred", 0xDB7093},
 {"papayawhip", 0xFFEFD5}, {"peachpuff", 0xFFDAB9},
 {"peru", 0xCD853F}, {"pink", 0xFFC0CB},
 {"plum", 0xDDA0DD}, {"powderblue", 0xB0E0E6},
 {"purple", 0x800080}, {"rebeccapurple", 0x663399},
 {"red", 0xFF0000}, {"rosybrown", 0xBC8F8F},
 {"royalblue", 0x4169E1}, {"saddlebrown", 0x8B4513},
 {"salmon", 0xFA8072}, {"sandybrown", 0xF4A460},
 {"seagreen", 0x2E8B57}, {"seashell", 0xFFF5EE},
 {"sienna", 0xA0522D}, {"silver", 0xC0C0C0},
 {"skyblue", 0x87CEEB}, {"slateblue", 0x6A5ACD},
 {"slategray", 0x708090}, {"slategrey", 0x708090},
 {"snow", 0xFFFAFA}, {"springgreen", 0x00FF7F},
 {"steelblue", 0x4682B4}, {"tan", 0xD2B48C},
 {"teal", 0x008080}, {"thistle", 0xD8BFD8},
 {"tomato", 0xFF6347}, {"turquoise", 0x40E0D0},
 {"violet", 0xEE82EE}, {"wheat", 0xF5DEB3},
 {"white", 0xFFFFFF}, {"whitesmoke", 0xF5F5F5},
 {"yellow", 0xFFFF00}, {"yellowgreen", 0x9ACD32}
 };
 return colors;
}
}
DisplayList DisplayListBuilder::build(const LayoutNode* root) {
 DisplayList displayList;
 buildNode(root, displayList, 0, 0, "black", 400);
 return displayList;
}

void DisplayListBuilder::buildNode(
    const LayoutNode* node,
    DisplayList& displayList,
    float parentX,
    float parentY,
    const std::string& inheritedColor,
    int inheritedWeight
){
    if(!node||!node->domNode)return;

    float absoluteX=parentX+node->x;
    float absoluteY=parentY+node->y;
    Node* domNode=node->domNode;
    
    std::string colorValue=inheritedColor;
    std::string tag;
    int fontWeight=inheritedWeight;

    if(domNode->type()==NodeType::Element){
        std::string computedColor=domNode->computedStyle("color");
        if(!computedColor.empty())colorValue=computedColor;
        std::string computedWeight=domNode->computedStyle("font-weight");
        if(computedWeight=="bold")fontWeight=700;
        else if(computedWeight=="normal")fontWeight=400;
        else if(!computedWeight.empty())fontWeight=static_cast<int>(parseNumber(computedWeight,inheritedWeight));
    }

    if(domNode->type()==NodeType::Element){
        tag=domNode->name();
        if(domNode->computedStyle("font-weight").empty()&&(tag=="h1"||tag=="h2"||tag=="h3"||tag=="h4"||tag=="h5"||tag=="h6"))fontWeight=700;
        Color foreground=parseColor(colorValue);
        std::string background=domNode->computedStyle("background");
        std::string backgroundColorValue=domNode->computedStyle("background-color");
        if(backgroundColorValue.empty()||backgroundColorValue=="transparent")
            backgroundColorValue=background;
        Color backgroundColor=backgroundColorValue.empty()?Color{0,0,0,0}:parseColor(backgroundColorValue,colorValue);

        if(backgroundColor.a>0&&node->width>0&&node->height>0)
            displayList.addRect(absoluteX,absoluteY,node->width,node->height,backgroundColor);

        bool control=tag=="input"||tag=="button"||tag=="textarea"||tag=="select";
        std::string border=domNode->computedStyle("border");
        const std::string sides[4]={"top","right","bottom","left"};
        const float widths[4]={node->borderTop,node->borderRight,node->borderBottom,node->borderLeft};
        Color borderColors[4];
        std::string borderStyles[4];
        std::string commonColor=domNode->computedStyle("border-color");
        std::string commonStyle=domNode->computedStyle("border-style");
        std::string shorthandColor=shorthandBorderColor(border);
        std::string shorthandStyle=shorthandBorderStyle(border);
        for(int i=0;i<4;i++){
            std::string sideColor=domNode->computedStyle(std::string("border-")+sides[i]+"-color");
            if(sideColor.empty())sideColor=borderBoxValue(commonColor,i);
            if(sideColor.empty())sideColor=shorthandColor;
            if(sideColor.empty())sideColor=control?"#767676":colorValue;
            borderColors[i]=parseColor(sideColor,colorValue);

            borderStyles[i]=domNode->computedStyle(std::string("border-")+sides[i]+"-style");
            if(borderStyles[i].empty())borderStyles[i]=borderBoxValue(commonStyle,i);
            if(borderStyles[i].empty())borderStyles[i]=shorthandStyle;
            if(borderStyles[i].empty())borderStyles[i]=control&&node->borderTop>0?"solid":"none";
        }
        addBorders(
            displayList,absoluteX,absoluteY,node->width,node->height,
            widths[0],widths[1],widths[2],widths[3],
            borderColors[0],borderColors[1],borderColors[2],borderColors[3],
            borderStyles[0],borderStyles[1],borderStyles[2],borderStyles[3]
        );
    
        if(tag=="hr"){
            Color ruleColor=parseColor(domNode->computedStyle("border-color"),colorValue);
            if(ruleColor.a==0)ruleColor=foreground;
            displayList.addElement(PaintElementType::HorizontalRule,absoluteX,absoluteY,node->width,node->height,ruleColor);
        }else if(tag=="img"){
            displayList.addElement(PaintElementType::Image,absoluteX,absoluteY,node->width,node->height,backgroundColor,domNode->attribute("src"),domNode->attribute("alt"));
        }else if(tag=="input"){
            std::string inputType=domNode->attribute("type");
            if(inputType.empty())inputType="text";
            Color inputColor=backgroundColor.a?backgroundColor:Color{255,255,255,255};
            displayList.addElement(PaintElementType::Input,absoluteX,absoluteY,node->width,node->height,inputColor,"","",domNode->attribute("value"),inputType);
        }else if(tag=="button"){
            Color buttonColor=backgroundColor.a?backgroundColor:Color{239,239,239,255};
            displayList.addElement(PaintElementType::Button,absoluteX,absoluteY,node->width,node->height,buttonColor,"","",domNode->attribute("value"),domNode->attribute("type"));
        }
    }

    if(domNode->type()==NodeType::Text){
        if(!node->textFragments.empty()){
            for(const auto& fragment:node->textFragments){
                if(fragment.text.empty())continue;

                displayList.addText(
                    absoluteX+fragment.x,
                    absoluteY+fragment.y+node->fontBaseline,
                    node->fontSize,
                    fontWeight,
                    fragment.text,
                    parseColor(colorValue)
                );
            }
        }else{
            const std::string& content=domNode->text();

            bool onlyWhitespace=std::all_of(
                content.begin(),
                content.end(),
                [](unsigned char c){return std::isspace(c);}
            );

            if(!content.empty()&&!onlyWhitespace){
                displayList.addText(
                    absoluteX,
                    absoluteY+node->fontBaseline,
                    node->fontSize,
                    fontWeight,
                    content,
                    parseColor(colorValue)
                );
            }
        }

        return;
    }

    for(const auto& child:node->children){
        buildNode(
            child.get(),
            displayList,
            absoluteX,
            absoluteY,
            colorValue,
            fontWeight
        );
    }
}

Color DisplayListBuilder::parseColor(
 const std::string& value,
 const std::string& currentColor
) {
 if (value == "currentColor") {
 return parseColor(currentColor, "black");
 }
 std::string color = trim(value);
 std::transform(
 color.begin(),
 color.end(),
 color.begin(),
 [](unsigned char c) {
 return static_cast<char>(std::tolower(c));
 }
 );
 if (color == "transparent") return {0, 0, 0, 0};
 if (color == "currentcolor") return {0, 0, 0, 255};
 auto named = namedColors().find(color);
 if (named != namedColors().end()) {
 uint32_t rgb = named->second;
 return {
 static_cast<uint8_t>((rgb >> 16) & 255),
 static_cast<uint8_t>((rgb >> 8) & 255),
 static_cast<uint8_t>(rgb & 255),
 255
 };
 }
 if (!color.empty() && color[0] == '#') {
 std::string hex = color.substr(1);
 try {
 if (hex.size() == 3 || hex.size() == 4) {
 int r = std::stoi(hex.substr(0, 1), nullptr, 16);
 int g = std::stoi(hex.substr(1, 1), nullptr, 16);
 int b = std::stoi(hex.substr(2, 1), nullptr, 16);
 int a = hex.size() == 4
 ? std::stoi(hex.substr(3, 1), nullptr, 16) * 17
 : 255;
 return {
 static_cast<uint8_t>(r * 17),
 static_cast<uint8_t>(g * 17),
 static_cast<uint8_t>(b * 17),
 static_cast<uint8_t>(a)
 };
 }
 if (hex.size() == 6 || hex.size() == 8) {
 unsigned long rgb = std::stoul(hex, nullptr, 16);
 if (hex.size() == 6) {
 return {
 static_cast<uint8_t>((rgb >> 16) & 255),
 static_cast<uint8_t>((rgb >> 8) & 255),
 static_cast<uint8_t>(rgb & 255),
 255
 };
 }
 return {
 static_cast<uint8_t>((rgb >> 24) & 255),
 static_cast<uint8_t>((rgb >> 16) & 255),
 static_cast<uint8_t>((rgb >> 8) & 255),
 static_cast<uint8_t>(rgb & 255)
 };
 }
 }
 catch (...) {
 return {0, 0, 0, 255};
 }
 }
 size_t open = color.find('(');
 size_t close = color.rfind(')');
 if (open != std::string::npos && close == color.size() - 1) {
 std::string function = color.substr(0, open);
 std::string arguments = color.substr(open + 1, close - open - 1);
 auto values = splitColorArguments(arguments);
 if ((function == "rgb" || function == "rgba") && values.size() >= 3) {
 return {
 channel(values[0]),
 channel(values[1]),
 channel(values[2]),
 values.size() >= 4 ? alphaChannel(values[3]) : static_cast<uint8_t>(255)
 };
 }
 if ((function == "hsl" || function == "hsla") && values.size() >= 3) {
 double hue = parseNumber(values[0]);
 double saturation = parseNumber(values[1]);
 double lightness = parseNumber(values[2]);
 double alpha = values.size() >= 4
 ? (values[3].find('%') != std::string::npos
 ? parseNumber(values[3]) / 100.0
 : parseNumber(values[3]))
 : 1.0;
 return hslToRgb(hue, saturation, lightness, alpha);
 }
 }
 return {0, 0, 0, 255};
}
}
