#include "layout_engine.h"
#include "../render/font_manager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>
#include <iostream>

namespace Veltrix {
namespace {

float parseNumber(const std::string& value,float fallback=0){
    if(value.empty()||value=="auto")return fallback;
    char* end=nullptr;
    float result=std::strtof(value.c_str(),&end);
    return end==value.c_str()?fallback:result;
}

float collapseMargins(float a,float b){
    return std::max(std::max(a,b),0.0f)+std::min(std::min(a,b),0.0f);
}

std::string lower(const std::string& value){
    std::string result=value;
    std::transform(result.begin(),result.end(),result.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return result;
}

bool isHTMLWhitespace(unsigned char c){
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f';
}

std::vector<float> parseBoxValues(const std::string& value){
    std::vector<float> values;
    std::string current;
    for(char c:value){
        if(std::isspace(static_cast<unsigned char>(c))){
            if(!current.empty()){
                values.push_back(parseNumber(current));
                current.clear();
            }
        }else current+=c;
    }
    if(!current.empty())values.push_back(parseNumber(current));
    return values;
}

void expandBox(const std::string& value,float& top,float& right,float& bottom,float& left){
    auto values=parseBoxValues(value);
    if(values.empty())top=right=bottom=left=0;
    else if(values.size()==1)top=right=bottom=left=values[0];
    else if(values.size()==2){
        top=bottom=values[0];
        right=left=values[1];
    }else if(values.size()==3){
        top=values[0];
        right=left=values[1];
        bottom=values[2];
    }else{
        top=values[0];
        right=values[1];
        bottom=values[2];
        left=values[3];
    }
}

void parseBox(Node* node,const char* property,float& top,float& right,float& bottom,float& left){
    if(!node)return;
    std::string value=node->computedStyle(property);
    if(!value.empty())expandBox(value,top,right,bottom,left);
}

void parseBorders(Node* node,float& top,float& right,float& bottom,float& left){
    if(!node)return;

    std::string shorthand=node->computedStyle("border");
    if(!shorthand.empty()){
        size_t position=shorthand.find_first_of("0123456789.");
        if(position!=std::string::npos){
            float width=parseNumber(shorthand.substr(position),0);
            top=right=bottom=left=width;
        }
    }

    std::string borderWidth=node->computedStyle("border-width");
    if(!borderWidth.empty())expandBox(borderWidth,top,right,bottom,left);

    std::string value=node->computedStyle("border-top-width");
    if(!value.empty())top=parseNumber(value,top);

    value=node->computedStyle("border-right-width");
    if(!value.empty())right=parseNumber(value,right);

    value=node->computedStyle("border-bottom-width");
    if(!value.empty())bottom=parseNumber(value,bottom);

    value=node->computedStyle("border-left-width");
    if(!value.empty())left=parseNumber(value,left);
}

std::string defaultDisplay(const std::string& tag){
    static const std::vector<std::string> inlineTags={
        "a","abbr","b","bdi","bdo","br","cite","code",
        "del","em","i","ins","kbd","label","mark","q","s",
        "samp","small","span","strong","sub","sup","time","u","var"
    };

    static const std::vector<std::string> inlineBlockTags={
        "button","img","input","select","textarea"
    };

    if(std::find(inlineBlockTags.begin(),inlineBlockTags.end(),tag)!=inlineBlockTags.end())
        return "inline-block";

    if(std::find(inlineTags.begin(),inlineTags.end(),tag)!=inlineTags.end())
        return "inline";

    return "block";
}

bool isInlineDisplay(const std::string& display){
    return display=="inline"||display=="inline-block";
}

bool isHiddenTag(const std::string& tag){
    return tag=="head"||tag=="style"||tag=="script"||
           tag=="title"||tag=="meta"||tag=="link";
}

size_t utf8Length(const std::string& text,size_t position){
    unsigned char c=static_cast<unsigned char>(text[position]);
    if(c<0x80)return 1;
    if((c&0xE0)==0xC0)return 2;
    if((c&0xF0)==0xE0)return 3;
    if((c&0xF8)==0xF0)return 4;
    return 1;
}

float fallbackTextWidth(const std::string& text,float fontSize){
    float width=0;
    for(unsigned char c:text)
        width+=(c==' '||c=='\t')?fontSize*0.28f:fontSize*0.55f;
    return width;
}

float actualFontBaseline(
    FontManager& fontManager,
    float fontSize,
    float lineHeight
){
    unsigned int size=static_cast<unsigned int>(
        std::max(1.0f,std::round(fontSize))
    );

    if(!fontManager.setSize(size))return 0;

    FT_Face face=fontManager.face();

    if(!face||!face->size){
        std::cout<<"[BASELINE] FreeType face unavailable\n";
        return 0;
    }

    float ascender=face->size->metrics.ascender/64.0f;
    float metricsHeight=face->size->metrics.height/64.0f;
    float baseline=(lineHeight-metricsHeight)*0.5f+ascender;

    return baseline;
}

float verticalAlignmentOffset(
    FontManager& fontManager,
    const std::string& value,
    float parentFontSize,
    float parentLineHeight,
    float childFontSize,
    float childLineHeight,
    float referenceLineHeight
){
    std::string align=lower(value);

    float parentBaseline=actualFontBaseline(
        fontManager,parentFontSize,parentLineHeight
    );

    float childBaseline=actualFontBaseline(
        fontManager,childFontSize,childLineHeight
    );

    if(align.empty()||align=="baseline")
        return parentBaseline-childBaseline;

    if(align=="top"||align=="text-top")
        return 0;

    if(align=="bottom"||align=="text-bottom")
        return referenceLineHeight-childLineHeight;

    if(align=="middle"){
        unsigned int size=static_cast<unsigned int>(
            std::max(1.0f,std::round(parentFontSize))
        );

        if(!fontManager.setSize(size))
            return parentBaseline-childBaseline;

        FT_Face face=fontManager.face();

        if(!face||FT_Load_Char(face,'x',FT_LOAD_DEFAULT)!=0)
            return parentBaseline-childBaseline;

        float xHeight=face->glyph->metrics.height/64.0f;
        float parentTarget=parentBaseline-xHeight*0.5f;
        float childCenter=childLineHeight*0.5f;

        return parentTarget-childCenter;
    }

    if(align=="super")return -childFontSize*0.4f;
    if(align=="sub")return childFontSize*0.2f;

    if(align.find('%')!=std::string::npos)
        return -parseNumber(align)*childLineHeight/100.0f;

    if(align.find("px")!=std::string::npos)
        return -parseNumber(align);

    return parentBaseline-childBaseline;
}

}

bool LayoutEngine::initialize(const std::string& fontPath){
    fontAttempted=true;
    fontReady=fontManager.initialize(fontPath);
    return fontReady;
}

void LayoutEngine::shutdown(){
    fontManager.shutdown();
    fontReady=false;
    fontAttempted=true;
}

float LayoutEngine::measureText(const std::string& text,float fontSize){
    if(fontReady){
        return fontManager.measureText(
            text,
            static_cast<unsigned int>(std::max(1.0f,fontSize))
        );
    }

    return fallbackTextWidth(text,fontSize);
}

void LayoutEngine::layoutText(LayoutNode& layout,const std::string& text,float availableWidth){
    layout.textFragments.clear();

    std::string word;
    std::string currentLine;

    float currentWidth=0;
    float maximumWidth=0;
    float y=0;

    const float spaceWidth=measureText(" ",layout.fontSize);

    auto finishLine=[&](){
        if(!currentLine.empty()){
            layout.textFragments.push_back({
                currentLine,
                0,
                y,
                currentWidth
            });

            maximumWidth=std::max(maximumWidth,currentWidth);
        }

        currentLine.clear();
        currentWidth=0;
        y+=layout.lineHeight;
    };

    auto appendWord=[&](const std::string& value){
        if(value.empty())return;

        float wordWidth=measureText(value,layout.fontSize);

        if(!currentLine.empty()&&availableWidth>0&&
           currentWidth+spaceWidth+wordWidth>availableWidth){
            finishLine();
        }

        if(availableWidth>0&&wordWidth>availableWidth){
            for(size_t position=0;position<value.size();){
                size_t length=std::min(
                    utf8Length(value,position),
                    value.size()-position
                );

                std::string character=value.substr(position,length);
                float characterWidth=measureText(character,layout.fontSize);

                if(!currentLine.empty()&&
                   currentWidth+characterWidth>availableWidth){
                    finishLine();
                }

                currentLine+=character;
                currentWidth+=characterWidth;
                position+=length;
            }

            return;
        }

        if(!currentLine.empty()){
            currentLine+=" ";
            currentWidth+=spaceWidth;
        }

        currentLine+=value;
        currentWidth+=wordWidth;
    };

    for(size_t position=0;position<text.size();){
        unsigned char c=static_cast<unsigned char>(text[position]);

        if(isHTMLWhitespace(c)){
            appendWord(word);
            word.clear();

            while(position<text.size()&&
                  isHTMLWhitespace(static_cast<unsigned char>(text[position])))
                ++position;

            continue;
        }

        size_t length=std::min(
            utf8Length(text,position),
            text.size()-position
        );

        word+=text.substr(position,length);
        position+=length;
    }

    appendWord(word);

    if(!currentLine.empty()||layout.textFragments.empty()){
        layout.textFragments.push_back({
            currentLine,
            0,
            y,
            currentWidth
        });

        maximumWidth=std::max(maximumWidth,currentWidth);
        y+=layout.lineHeight;
    }

    layout.contentWidth=maximumWidth;
    layout.width=maximumWidth;
    layout.contentHeight=y;
    layout.height=y;
}

std::unique_ptr<LayoutNode> LayoutEngine::build(Node* root,float viewportWidth){
    if(!root)return nullptr;

    if(!fontAttempted)
        initialize("assets/fonts/Roboto-Regular.ttf");

    return buildNode(root,viewportWidth,16);
}

std::unique_ptr<LayoutNode> LayoutEngine::buildNode(
    Node* node,
    float availableWidth,
    float inheritedFontSize
){
    if(!node)return nullptr;

    auto layout=std::make_unique<LayoutNode>();

    layout->domNode=node;
    layout->fontSize=inheritedFontSize;
    layout->lineHeight=inheritedFontSize*1.2f;

    if(node->type()==NodeType::Text){
        layout->display="inline";

        layout->fontBaseline=actualFontBaseline(
            fontManager,
            layout->fontSize,
            layout->lineHeight
        );

        return layout;
    }

    if(node->type()!=NodeType::Element)
        return layout;

    std::string tag=lower(node->name());

    if(isHiddenTag(tag)){
        layout->display="none";
        return layout;
    }

    std::string display=node->computedStyle("display");

    if(display.empty())
        display=defaultDisplay(tag);

    layout->display=display;

    if(display=="none")
        return layout;

    std::string fontSizeValue=node->computedStyle("font-size");

    if(!fontSizeValue.empty())
        layout->fontSize=parseNumber(fontSizeValue,inheritedFontSize);

    if(layout->fontSize<=0)
        layout->fontSize=inheritedFontSize;

    layout->lineHeight=layout->fontSize*1.2f;

    layout->fontBaseline=actualFontBaseline(
        fontManager,
        layout->fontSize,
        layout->lineHeight
    );

    std::string lineHeightValue=node->computedStyle("line-height");

    if(!lineHeightValue.empty()){
        float parsed=parseNumber(lineHeightValue,layout->lineHeight);

        if(lineHeightValue.find("px")!=std::string::npos)
            layout->lineHeight=parsed;
        else if(parsed>0)
            layout->lineHeight=parsed*layout->fontSize;
    }

    parseBox(
        node,
        "margin",
        layout->marginTop,
        layout->marginRight,
        layout->marginBottom,
        layout->marginLeft
    );

    parseBox(
        node,
        "padding",
        layout->paddingTop,
        layout->paddingRight,
        layout->paddingBottom,
        layout->paddingLeft
    );

    parseBorders(
        node,
        layout->borderTop,
        layout->borderRight,
        layout->borderBottom,
        layout->borderLeft
    );

    std::string value;

    value=node->computedStyle("margin-top");
    if(!value.empty())
        layout->marginTop=parseNumber(value,layout->marginTop);

    value=node->computedStyle("margin-right");
    if(!value.empty())
        layout->marginRight=parseNumber(value,layout->marginRight);

    value=node->computedStyle("margin-bottom");
    if(!value.empty())
        layout->marginBottom=parseNumber(value,layout->marginBottom);

    value=node->computedStyle("margin-left");
    if(!value.empty())
        layout->marginLeft=parseNumber(value,layout->marginLeft);

    value=node->computedStyle("padding-top");
    if(!value.empty())
        layout->paddingTop=parseNumber(value,layout->paddingTop);

    value=node->computedStyle("padding-right");
    if(!value.empty())
        layout->paddingRight=parseNumber(value,layout->paddingRight);

    value=node->computedStyle("padding-bottom");
    if(!value.empty())
        layout->paddingBottom=parseNumber(value,layout->paddingBottom);

    value=node->computedStyle("padding-left");
    if(!value.empty())
        layout->paddingLeft=parseNumber(value,layout->paddingLeft);

    float horizontalExtra=
        layout->paddingLeft+
        layout->paddingRight+
        layout->borderLeft+
        layout->borderRight;

    float verticalExtra=
        layout->paddingTop+
        layout->paddingBottom+
        layout->borderTop+
        layout->borderBottom;

    std::string boxSizing=node->computedStyle("box-sizing");
    std::string widthValue=node->computedStyle("width");
    std::string heightValue=node->computedStyle("height");

    bool widthAuto=widthValue.empty()||widthValue=="auto";
    bool heightAuto=heightValue.empty()||heightValue=="auto";
    bool inlineDisplay=isInlineDisplay(display);

    if(tag=="img"&&widthAuto&&node->hasAttribute("width"))
        widthValue=node->attribute("width");

    if(tag=="img"&&heightAuto&&node->hasAttribute("height"))
        heightValue=node->attribute("height");

    if(tag=="input"&&widthAuto)
        widthValue="180";

    if(tag=="button"&&widthAuto)
        widthValue="90";

    if(tag=="textarea"&&widthAuto)
        widthValue="300";

    if(tag=="select"&&widthAuto)
        widthValue="160";

    if(tag=="input"&&heightAuto)
        heightValue="30";

    if(tag=="button"&&heightAuto)
        heightValue="32";

    if(tag=="textarea"&&heightAuto)
        heightValue="100";

    if(tag=="select"&&heightAuto)
        heightValue="30";

    if(tag=="img"&&heightAuto){
        if(node->hasAttribute("height"))
            heightValue=node->attribute("height");
        else
            heightValue="150";
    }

    if(tag=="hr"&&heightAuto)
        heightValue="1";

    widthAuto=widthValue.empty()||widthValue=="auto";
    heightAuto=heightValue.empty()||heightValue=="auto";

    float specifiedWidth=0;

    if(widthAuto){
        if(inlineDisplay){
            if(tag=="img")specifiedWidth=150;
            else if(tag=="input")specifiedWidth=180;
            else if(tag=="button")specifiedWidth=90;
            else if(tag=="textarea")specifiedWidth=300;
            else if(tag=="select")specifiedWidth=160;
            else specifiedWidth=availableWidth;
        }else{
            specifiedWidth=std::max(
                0.0f,
                availableWidth-
                layout->marginLeft-
                layout->marginRight
            );
        }
    }else{
        specifiedWidth=parseNumber(widthValue,availableWidth);
    }

    if(boxSizing=="border-box"){
        layout->width=specifiedWidth;
        layout->contentWidth=std::max(
            0.0f,
            specifiedWidth-horizontalExtra
        );
    }else{
        layout->contentWidth=std::max(0.0f,specifiedWidth);
        layout->width=layout->contentWidth+horizontalExtra;
    }

    if(inlineDisplay&&widthAuto&&
       tag!="img"&&tag!="input"&&tag!="button"&&
       tag!="textarea"&&tag!="select"){
        layout->contentWidth=0;
        layout->width=horizontalExtra;
    }

    std::string minWidth=node->computedStyle("min-width");
    std::string maxWidth=node->computedStyle("max-width");

    if(!minWidth.empty()){
        float minimum=parseNumber(minWidth,0);

        if(boxSizing=="border-box"){
            layout->width=std::max(layout->width,minimum);
            layout->contentWidth=std::max(
                0.0f,
                layout->width-horizontalExtra
            );
        }else{
            layout->contentWidth=std::max(
                layout->contentWidth,
                minimum
            );

            layout->width=layout->contentWidth+horizontalExtra;
        }
    }

    if(!maxWidth.empty()){
        float maximum=parseNumber(maxWidth,layout->width);

        layout->width=std::min(layout->width,maximum);

        layout->contentWidth=std::max(
            0.0f,
            layout->width-horizontalExtra
        );
    }

    float contentAvailableWidth=std::max(
        0.0f,
        layout->contentWidth
    );

    float contentStartX=
        layout->borderLeft+
        layout->paddingLeft;

    float contentStartY=
        layout->borderTop+
        layout->paddingTop;

    for(const auto& child:node->children()){
        auto childLayout=buildNode(
            child.get(),
            contentAvailableWidth>0?
                contentAvailableWidth:
                availableWidth,
            layout->fontSize
        );

        if(!childLayout||childLayout->display=="none")
            continue;

        if(childLayout->domNode&&
           childLayout->domNode->type()==NodeType::Text){
            childLayout->fontSize=layout->fontSize;
            childLayout->lineHeight=layout->lineHeight;

            childLayout->fontBaseline=actualFontBaseline(
                fontManager,
                childLayout->fontSize,
                childLayout->lineHeight
            );
        }

        layout->children.push_back(std::move(childLayout));
    }

    struct InlineFlow{
        float x=0;
        float y=0;
        float lineHeight=0;
        float maximumWidth=0;
        float lineStartX=0;
        bool pendingSpace=false;
    };

    InlineFlow flow;

    const float flowWidth=
        contentAvailableWidth>0?
            contentAvailableWidth:
            availableWidth;

    auto finishLine=[&](){
        flow.y+=std::max(
            flow.lineHeight,
            layout->lineHeight
        );

        flow.x=flow.lineStartX;
        flow.lineHeight=0;
        flow.pendingSpace=false;
    };

    std::function<void(LayoutNode*,float,float,float,float)> placeInline;

    placeInline=[&](
        LayoutNode* item,
        float parentX,
        float parentY,
        float parentFontSize,
        float parentLineHeight
    ){
        if(!item||!item->domNode)
            return;

        if(item->domNode->type()==NodeType::Text){
            const std::string& text=item->domNode->text();

            float startX=flow.x;
            float startY=flow.y;

            item->x=flow.x-parentX;
            item->y=flow.y-parentY;

            item->textFragments.clear();

            std::string word;

            auto emitWord=[&](){
                if(word.empty())
                    return;

                float wordWidth=measureText(
                    word,
                    item->fontSize
                );

                float spaceWidth=measureText(
                    " ",
                    item->fontSize
                );

                if(flow.pendingSpace&&flow.x>0){
                    if(flowWidth>0&&
                       flow.x+spaceWidth+wordWidth>flowWidth){
                        finishLine();
                    }else{
                        flow.x+=spaceWidth;
                    }
                }

                flow.pendingSpace=false;

                if(flowWidth>0&&flow.x>0&&
                   flow.x+wordWidth>flowWidth){
                    finishLine();
                }

                if(flowWidth>0&&wordWidth>flowWidth){
                    for(size_t position=0;
                        position<word.size();){

                        size_t length=std::min(
                            utf8Length(word,position),
                            word.size()-position
                        );

                        std::string character=
                            word.substr(position,length);

                        float characterWidth=measureText(
                            character,
                            item->fontSize
                        );

                        if(flow.x>0&&
                           flow.x+characterWidth>flowWidth)
                            finishLine();

                        item->textFragments.push_back({
                            character,
                            flow.x-startX,
                            flow.y-startY,
                            characterWidth
                        });

                        flow.x+=characterWidth;

                        flow.lineHeight=std::max(
                            flow.lineHeight,
                            item->lineHeight
                        );

                        flow.maximumWidth=std::max(
                            flow.maximumWidth,
                            flow.x
                        );

                        position+=length;
                    }

                    return;
                }

                item->textFragments.push_back({
                    word,
                    flow.x-startX,
                    flow.y-startY,
                    wordWidth
                });

                flow.x+=wordWidth;

                flow.lineHeight=std::max(
                    flow.lineHeight,
                    item->lineHeight
                );

                flow.maximumWidth=std::max(
                    flow.maximumWidth,
                    flow.x
                );
            };

            for(size_t position=0;position<text.size();){
                unsigned char c=
                    static_cast<unsigned char>(text[position]);

                if(isHTMLWhitespace(c)){
                    emitWord();
                    word.clear();
                    flow.pendingSpace=true;

                    while(
                        position<text.size()&&
                        isHTMLWhitespace(
                            static_cast<unsigned char>(
                                text[position]
                            )
                        )
                    )
                        ++position;

                    continue;
                }

                size_t length=std::min(
                    utf8Length(text,position),
                    text.size()-position
                );

                word+=text.substr(position,length);
                position+=length;
            }

            emitWord();

            item->width=std::max(
                0.0f,
                flow.x-startX
            );

            item->contentWidth=item->width;

            item->height=std::max(
                item->lineHeight,
                flow.y-startY+item->lineHeight
            );

            item->contentHeight=item->height;

            return;
        }

        std::string itemTag=lower(
            item->domNode->name()
        );

        if(itemTag=="br"){
            item->x=flow.x-parentX;
            item->y=flow.y-parentY;

            finishLine();

            item->width=0;
            item->height=item->lineHeight;
            item->contentWidth=0;
            item->contentHeight=item->height;

            return;
        }

        float startX=flow.x;
        float startY=flow.y;
        float outerFlowY=startY;
        float outerLineStartX=flow.lineStartX;
        float outerMaximumWidth=flow.maximumWidth;
        flow.maximumWidth=flow.x;

        float referenceLineHeight=std::max(
            parentLineHeight,
            flow.lineHeight
        );

        referenceLineHeight=std::max(
            referenceLineHeight,
            item->lineHeight
        );

        std::string rawVerticalAlign=
            item->domNode->computedStyle(
                "vertical-align"
            );

        float alignmentOffset=
            verticalAlignmentOffset(
                fontManager,
                rawVerticalAlign,
                parentFontSize,
                parentLineHeight,
                item->fontSize,
                item->lineHeight,
                referenceLineHeight
            );

        item->x=
            startX-parentX+
            item->marginLeft;

        item->y=
            startY-parentY+
            item->marginTop+
            alignmentOffset;

        float elementOriginX=
            startX+
            item->marginLeft;

        float elementOriginY=
            startY+
            item->marginTop+
            alignmentOffset;

        float childOriginX=
            elementOriginX+
            item->paddingLeft+
            item->borderLeft;

        float childOriginY=
            elementOriginY+
            item->paddingTop+
            item->borderTop;

        flow.y=childOriginY;
        flow.x=childOriginX;
        flow.lineStartX=childOriginX;

        float previousBlockMargin=0;
        bool previousBlock=false;

        for(const auto& child:item->children){
            if(!child||child->display=="none")
                continue;

                if(child->domNode&&
                    child->domNode->type()==NodeType::Text){
      
                     if(previousBlock){
                         flow.y+=previousBlockMargin;
                         previousBlock=false;
                         previousBlockMargin=0;
                     }
      
                     placeInline(
                         child.get(),
                         childOriginX,
                         childOriginY,
                         item->fontSize,
                         item->lineHeight
                     );
      
                 }else if(isInlineDisplay(child->display)){
      
                     if(previousBlock){
                         flow.y+=previousBlockMargin;
                         previousBlock=false;
                         previousBlockMargin=0;
                     }
      
                     placeInline(
                         child.get(),
                         childOriginX,
                         childOriginY,
                         item->fontSize,
                         item->lineHeight
                     );
      
                 }else{
                     if(flow.x>childOriginX||flow.lineHeight>0)
                         finishLine();
      
                     flow.pendingSpace=false;
      
                     float collapsedTop=previousBlock?
                         collapseMargins(previousBlockMargin,child->marginTop):
                         child->marginTop;
      
                     child->x=
                         item->paddingLeft+
                         item->borderLeft+
                         child->marginLeft;
      
                     child->y=
                         flow.y-
                         elementOriginY+
                         collapsedTop;
      
                     flow.y+=collapsedTop+child->height;
      
                     previousBlockMargin=child->marginBottom;
                     previousBlock=true;
                 }
        }
        
        if(previousBlock)
        flow.y+=previousBlockMargin;

        
        float innerMaximumX=std::max(flow.x,flow.maximumWidth);
        flow.maximumWidth=std::max(outerMaximumWidth,innerMaximumX);

        float consumedWidth=
            std::max(flow.x,innerMaximumX)-
            elementOriginX;

        float consumedHeight=
            flow.y-
            elementOriginY;

            item->width=std::max(
                item->width,
                consumedWidth+
                item->paddingRight+
                item->borderRight
            );

            item->height=std::max(
                item->height,
                consumedHeight+
                item->paddingBottom+
                item->borderBottom
            );

        item->contentWidth=std::max(
            0.0f,
            item->width-
            item->paddingLeft-
            item->paddingRight-
            item->borderLeft-
            item->borderRight
        );

        item->contentHeight=std::max(
            0.0f,
            item->height-
            item->paddingTop-
            item->paddingBottom-
            item->borderTop-
            item->borderBottom
        );

        flow.y=outerFlowY+std::max(0.0f,flow.y-childOriginY);
        flow.lineStartX=outerLineStartX;

        float verticalExtent=std::max(
            item->height+alignmentOffset,
            referenceLineHeight
        );
        
        flow.lineHeight=std::max(
            flow.lineHeight,
            verticalExtent+
            item->marginTop+
            item->marginBottom
        );
        
        flow.x=std::max(
            flow.x,
            elementOriginX+item->width
        );
        
        flow.maximumWidth=std::max(
            flow.maximumWidth,
            flow.x
        );
        
        flow.x+=item->marginRight;
        };

        float previousBlockMargin=0;
        bool previousBlock=false;

        for(const auto& child:layout->children){
            if(!child||child->display=="none")
                continue;
     
            bool textNode=
                child->domNode&&
                child->domNode->type()==NodeType::Text;
     
            if(textNode||isInlineDisplay(child->display)){
                if(previousBlock){
                    flow.y+=previousBlockMargin;
                    previousBlock=false;
                    previousBlockMargin=0;
                }
     
                placeInline(
                    child.get(),
                    -contentStartX,
                    -contentStartY,
                    layout->fontSize,
                    layout->lineHeight
                );
            }else{
                if(flow.x>0||flow.lineHeight>0)
                    finishLine();
     
                flow.pendingSpace=false;
     
                float collapsedTop=previousBlock?
                    collapseMargins(previousBlockMargin,child->marginTop):
                    child->marginTop;
     
                child->x=
                    contentStartX+
                    child->marginLeft;
     
                child->y=
                    contentStartY+
                    flow.y+
                    collapsedTop;
     
                flow.y+=collapsedTop+child->height;
     
                previousBlockMargin=child->marginBottom;
                previousBlock=true;
     
                flow.maximumWidth=std::max(
                    flow.maximumWidth,
                    child->width
                );
            }
        }
     
        if(previousBlock)
            flow.y+=previousBlockMargin;

    if(flow.x>0||flow.lineHeight>0){
        flow.y+=std::max(
            flow.lineHeight,
            layout->lineHeight
        );
    }

    float naturalHeight=std::max(
        flow.y,
        calculateHeight(node)
    );

    if(!heightValue.empty()&&heightValue!="auto"){
        float specifiedHeight=parseNumber(
            heightValue,
            naturalHeight
        );

        if(boxSizing=="border-box"){
            layout->height=std::max(
                0.0f,
                specifiedHeight
            );

            layout->contentHeight=std::max(
                0.0f,
                layout->height-verticalExtra
            );
        }else{
            layout->contentHeight=std::max(
                0.0f,
                specifiedHeight
            );

            layout->height=
                layout->contentHeight+
                verticalExtra;
        }
    }else{
        layout->contentHeight=naturalHeight;

        layout->height=
            layout->contentHeight+
            verticalExtra;
    }

    std::string minHeight=
        node->computedStyle("min-height");

    std::string maxHeight=
        node->computedStyle("max-height");

    if(!minHeight.empty()){
        float minimum=parseNumber(
            minHeight,
            0
        );

        if(boxSizing=="border-box"){
            layout->height=std::max(
                layout->height,
                minimum
            );

            layout->contentHeight=std::max(
                0.0f,
                layout->height-verticalExtra
            );
        }else{
            layout->contentHeight=std::max(
                layout->contentHeight,
                minimum
            );

            layout->height=
                layout->contentHeight+
                verticalExtra;
        }
    }

    if(!maxHeight.empty()){
        float maximum=parseNumber(
            maxHeight,
            layout->height
        );

        layout->height=
            std::min(
                layout->height,
                maximum
            );

        layout->contentHeight=std::max(
            0.0f,
            layout->height-verticalExtra
        );
    }

    return layout;
}

float LayoutEngine::calculateHeight(Node* node){
    if(!node||node->type()!=NodeType::Element)
        return 0;

    std::string tag=lower(node->name());

    if(tag=="h1")return 38;
    if(tag=="h2")return 32;
    if(tag=="h3")return 28;
    if(tag=="h4")return 26;
    if(tag=="h5")return 24;
    if(tag=="h6")return 22;
    if(tag=="p")return 24;
    if(tag=="li")return 24;
    if(tag=="br")return 16;
    if(tag=="hr")return 1;

    if(tag=="img"){
        if(node->hasAttribute("height"))
            return parseDimension(
                node->attribute("height"),
                150
            );

        return 150;
    }

    if(tag=="input")return 30;
    if(tag=="button")return 32;
    if(tag=="textarea")return 100;
    if(tag=="select")return 30;

    return 0;
}

float LayoutEngine::parseDimension(
    const std::string& value,
    float fallback
){
    return parseNumber(value,fallback);
}

}