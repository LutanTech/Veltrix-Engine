#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <utility>
#include <SDL3_image/SDL_image.h>

namespace Veltrix {

namespace {

bool isUtf8Continuation(unsigned char c){return(c&0xC0)==0x80;}

size_t nextUtf8(const std::string& text,size_t offset){
    if(offset>=text.size())return text.size();
    unsigned char c=static_cast<unsigned char>(text[offset]);
    size_t length=1;
    if((c&0x80)==0)length=1;
    else if((c&0xE0)==0xC0)length=2;
    else if((c&0xF0)==0xE0)length=3;
    else if((c&0xF8)==0xF0)length=4;
    if(offset+length>text.size())return text.size();
    for(size_t i=1;i<length;++i)
        if(!isUtf8Continuation(static_cast<unsigned char>(text[offset+i])))return offset+1;
    return offset+length;
}

size_t previousUtf8(const std::string& text,size_t offset){
    if(offset==0||text.empty())return 0;
    size_t position=offset-1;
    while(position>0&&isUtf8Continuation(static_cast<unsigned char>(text[position])))--position;
    return position;
}

bool isWordCharacter(const std::string& text,size_t offset){
    if(offset>=text.size())return false;
    size_t next=nextUtf8(text,offset);
    if(next==offset)return false;
    unsigned char c=static_cast<unsigned char>(text[offset]);
    return c<128?std::isalnum(c)||c=='_':true;
}

bool isWhitespaceCharacter(const std::string& text,size_t offset){
    if(offset>=text.size())return false;
    unsigned char c=static_cast<unsigned char>(text[offset]);
    return c<128?std::isspace(c):false;
}

}

bool Engine::initialize(){
    if(!SDL_Init(SDL_INIT_VIDEO))return false;
    SDL_DisplayID display=SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode=SDL_GetCurrentDisplayMode(display);
    int width=mode?static_cast<int>(mode->w*.85f):1100;
    int height=mode?static_cast<int>(mode->h*.85f):700;
    window=SDL_CreateWindow("Veltrix | Engineered to render.",width,height,SDL_WINDOW_RESIZABLE);
    if(!window){SDL_Quit();return false;}
    SDL_GetWindowSize(window,&width,&height);
    viewportWidth=static_cast<float>(width);
    viewportHeight=static_cast<float>(height);
    renderer=SDL_CreateRenderer(window,nullptr);
    if(!renderer){SDL_DestroyWindow(window);SDL_Quit();return false;}
    if(!textRenderer.initialize(renderer,"assets/fonts/Roboto-Regular.ttf")){
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    arrowCursor=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    textCursor=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    linkCursor=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    running=true;
    return true;
}

void Engine::setResizeHandler(std::function<DisplayList(float,float)> handler){resizeHandler=std::move(handler);}

void Engine::getViewportSize(float& width,float& height) const{
    width=viewportWidth;
    height=viewportHeight;
}

void Engine::setDisplayList(const DisplayList& list){
    displayList=list;
    inputValues.clear();
    for(const auto& element:displayList.elements())
        if(element.type==PaintElementType::Input)
            inputValues.push_back(element.value);
    scrollOffset=0;
    targetScrollOffset=0;
    clearSelection();
    updateContentHeight();
}

void Engine::updateContentHeight(){
    contentHeight=0;
    for(const auto& rect:displayList.rectangles())
        contentHeight=std::max(contentHeight,rect.y+rect.height);
    for(const auto& text:displayList.texts()){
        float h=textRenderer.lineHeight(text.size);
        contentHeight=std::max(contentHeight,text.y+h*.25f);
    }
    for(const auto& element:displayList.elements())
    contentHeight=std::max(contentHeight,element.y+element.height);
    int width=0,height=0;
    if(window&&SDL_GetWindowSize(window,&width,&height))
        viewportHeight=static_cast<float>(height);
    float maxScroll=std::max(0.0f,contentHeight-viewportHeight);
    scrollOffset=std::clamp(scrollOffset,0.0f,maxScroll);
    targetScrollOffset=std::clamp(targetScrollOffset,0.0f,maxScroll);
}

void Engine::handleScroll(float amount){
    float maxScroll=std::max(0.0f,contentHeight-viewportHeight);
    targetScrollOffset=std::clamp(targetScrollOffset-amount*scrollSpeed,0.0f,maxScroll);
}

void Engine::setTextCursor(bool text){
    SDL_Cursor* cursor=text?textCursor:arrowCursor;
    if(cursor)SDL_SetCursor(cursor);
}

float Engine::textWidth(const PaintText& text,size_t byteOffset){
    return textRenderer.measureText(text.text.substr(0,std::min(byteOffset,text.text.size())),text.size);
}

size_t Engine::characterOffsetAt(const PaintText& text,float localX){
    if(text.text.empty()||localX<=0)return 0;
    size_t offset=0;
    while(offset<text.text.size()){
        size_t next=nextUtf8(text.text,offset);
        float left=textWidth(text,offset);
        float right=textWidth(text,next);
        if(localX<left+(right-left)*.5f)return offset;
        offset=next;
    }
    return text.text.size();
}

bool Engine::hitTestText(float mouseX,float mouseY,int& itemIndex,size_t& characterOffset){
    const auto& texts=displayList.texts();
    float bestVertical=std::numeric_limits<float>::max();
    float bestHorizontal=std::numeric_limits<float>::max();
    int bestItem=-1;
    size_t bestOffset=0;

    for(size_t i=0;i<texts.size();++i){
        const auto& text=texts[i];
        float h=textRenderer.lineHeight(text.size);
        float top=text.y-scrollOffset-h*.82f;
        float bottom=text.y-scrollOffset+h*.25f;
        float verticalDistance=mouseY<top?top-mouseY:mouseY>bottom?mouseY-bottom:0;
        if(verticalDistance>h*.75f||verticalDistance>bestVertical)continue;

        float width=textWidth(text,text.text.size());
        float localX=mouseX-text.x;
        constexpr float textHitPadding=4.0f;
        if(localX< -textHitPadding||localX>width+textHitPadding)continue;
        float horizontalDistance=0;

        if(verticalDistance<bestVertical||(verticalDistance==bestVertical&&horizontalDistance<bestHorizontal)){
            bestVertical=verticalDistance;
            bestHorizontal=horizontalDistance;
            bestItem=static_cast<int>(i);
            bestOffset=characterOffsetAt(text,std::clamp(localX,0.0f,width));
        }
    }

    if(bestItem<0)return false;
    itemIndex=bestItem;
    characterOffset=bestOffset;
    return true;
}

void Engine::clearSelection(){
    textSelecting=false;
    textFocus=false;
    selectionAnchorItem=-1;
    selectionAnchorOffset=0;
    selectionFocusItem=-1;
    selectionFocusOffset=0;
}

bool Engine::hasSelection() const{
    return selectionAnchorItem>=0&&selectionFocusItem>=0&&
        (selectionAnchorItem!=selectionFocusItem||selectionAnchorOffset!=selectionFocusOffset);
}

void Engine::normalizeSelection(int& startItem,size_t& startOffset,int& endItem,size_t& endOffset) const{
    startItem=selectionAnchorItem;
    startOffset=selectionAnchorOffset;
    endItem=selectionFocusItem;
    endOffset=selectionFocusOffset;
    if(startItem>endItem||(startItem==endItem&&startOffset>endOffset)){
        std::swap(startItem,endItem);
        std::swap(startOffset,endOffset);
    }
}

std::string Engine::selectedText() const{
    if(!hasSelection())return "";
    int startItem,endItem;
    size_t startOffset,endOffset;
    normalizeSelection(startItem,startOffset,endItem,endOffset);
    const auto& texts=displayList.texts();
    if(startItem<0||endItem<0||startItem>=static_cast<int>(texts.size())||endItem>=static_cast<int>(texts.size()))return "";
    std::string result;
    for(int i=startItem;i<=endItem;++i){
        const std::string& value=texts[i].text;
        size_t from=i==startItem?startOffset:0;
        size_t to=i==endItem?endOffset:value.size();
        from=std::min(from,value.size());
        to=std::min(to,value.size());
        if(from>to)std::swap(from,to);
        result+=value.substr(from,to-from);
        if(i!=endItem)result+='\n';
    }
    return result;
}

void Engine::copySelection(){
    if(!hasSelection())return;
    std::string value=selectedText();
    if(!value.empty())SDL_SetClipboardText(value.c_str());
}

void Engine::selectWord(int itemIndex,size_t offset){
    const auto& texts=displayList.texts();
    if(itemIndex<0||itemIndex>=static_cast<int>(texts.size()))return;
    const std::string& text=texts[itemIndex].text;
    offset=std::min(offset,text.size());
    if(offset==text.size()&&offset>0)offset=previousUtf8(text,offset);
    if(offset>=text.size()){
        selectionAnchorItem=selectionFocusItem=itemIndex;
        selectionAnchorOffset=selectionFocusOffset=text.size();
        return;
    }
    bool word=isWordCharacter(text,offset);
    bool whitespace=isWhitespaceCharacter(text,offset);
    size_t start=offset,end=nextUtf8(text,offset);
    while(start>0){
        size_t previous=previousUtf8(text,start);
        if(isWordCharacter(text,previous)!=word||isWhitespaceCharacter(text,previous)!=whitespace)break;
        start=previous;
    }
    while(end<text.size()){
        if(isWordCharacter(text,end)!=word||isWhitespaceCharacter(text,end)!=whitespace)break;
        end=nextUtf8(text,end);
    }
    selectionAnchorItem=selectionFocusItem=itemIndex;
    selectionAnchorOffset=start;
    selectionFocusOffset=end;
    textFocus=true;
    caretStartTime=SDL_GetTicks();
}

void Engine::selectLine(int itemIndex){
    const auto& texts=displayList.texts();
    if(itemIndex<0||itemIndex>=static_cast<int>(texts.size()))return;

    int start=itemIndex,end=itemIndex;
    float lineHeight=textRenderer.lineHeight(texts[itemIndex].size);

    while(start>0){
        const auto& current=texts[start];
        const auto& previous=texts[start-1];
        float gap=current.y-previous.y;
        float expected=(lineHeight+textRenderer.lineHeight(previous.size))*.5f;
        if(gap>expected*.5f)break;
        --start;
    }

    while(end+1<static_cast<int>(texts.size())){
        const auto& current=texts[end];
        const auto& next=texts[end+1];
        float gap=next.y-current.y;
        float expected=(textRenderer.lineHeight(current.size)+textRenderer.lineHeight(next.size))*.5f;
        if(gap>expected*.5f)break;
        ++end;
    }

    selectionAnchorItem=start;
    selectionAnchorOffset=0;
    selectionFocusItem=end;
    selectionFocusOffset=texts[end].text.size();
    textFocus=true;
    caretStartTime=SDL_GetTicks();
}



void Engine::handleTextMouse(const SDL_Event& event){
    if(event.type==SDL_EVENT_MOUSE_MOTION){
        selectionMouseX=event.motion.x;
        selectionMouseY=event.motion.y;
        for(auto it=displayList.elements().rbegin();it!=displayList.elements().rend();++it){
            if(it->type!=PaintElementType::Link)continue;
            float y=it->y-scrollOffset;
            if(event.motion.x>=it->x&&event.motion.x<=it->x+it->width&&event.motion.y>=y&&event.motion.y<=y+it->height){
                if(linkCursor)SDL_SetCursor(linkCursor);
                return;
            }
        }
        for(const auto& element:displayList.elements()){
            if(element.type!=PaintElementType::Input&&element.type!=PaintElementType::Button)continue;
            float y=element.y-scrollOffset;
            if(event.motion.x>=element.x&&event.motion.x<=element.x+element.width&&event.motion.y>=y&&event.motion.y<=y+element.height){
                setTextCursor(element.type==PaintElementType::Input);
                return;
            }
        }
        int itemIndex;
        size_t offset;
        if(hitTestText(event.motion.x,event.motion.y,itemIndex,offset))setTextCursor(true);
        else if(!textSelecting)setTextCursor(false);
        if(textSelecting)updateTextSelection();
        return;
    }

    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_LEFT){
        float mouseX=event.button.x,mouseY=event.button.y;
        selectionMouseX=mouseX;
        selectionMouseY=mouseY;
        for(auto it=displayList.elements().rbegin();it!=displayList.elements().rend();++it){
            if(it->type!=PaintElementType::Link)continue;
            float y=it->y-scrollOffset;
            if(mouseX>=it->x&&mouseX<=it->x+it->width&&mouseY>=y&&mouseY<=y+it->height){
                clearSelection();
                textSelecting=false;
                textFocus=false;
                setTextCursor(false);
                if(!it->value.empty()&&it->value[0]=='#'){
                    std::string target=it->value.substr(1);
                    if(target.empty()||target=="top")targetScrollOffset=0;
                    else if(target=="bottom")targetScrollOffset=std::max(0.0f,contentHeight-viewportHeight);
                    else for(const auto& anchor:displayList.elements()){
                        if(anchor.type==PaintElementType::Anchor&&anchor.value==target){
                            targetScrollOffset=std::clamp(anchor.y,0.0f,std::max(0.0f,contentHeight-viewportHeight));
                            break;
                        }
                    }
                }else if(!it->value.empty())SDL_OpenURL(it->value.c_str());
                return;
            }
        }
        for(const auto& element:displayList.elements()){
            if(element.type!=PaintElementType::Button)continue;
            float y=element.y-scrollOffset;
            if(mouseX>=element.x&&mouseX<=element.x+element.width&&mouseY>=y&&mouseY<=y+element.height){
                clearSelection();
                setTextCursor(false);
                return;
            }
        }
        int itemIndex;
        size_t offset;
        if(!hitTestText(mouseX,mouseY,itemIndex,offset)){
            clearSelection();
            setTextCursor(false);
            return;
        }
        setTextCursor(true);
        Uint64 now=SDL_GetTicks();
        float dx=mouseX-lastClickX,dy=mouseY-lastClickY;
        clickCount=(now-lastClickTime<=500&&std::abs(dx)<5&&std::abs(dy)<5)?clickCount+1:1;
        lastClickTime=now;
        lastClickX=mouseX;
        lastClickY=mouseY;
        bool shift=(SDL_GetModState()&SDL_KMOD_SHIFT)!=0;

        if(clickCount>=3){
            selectLine(itemIndex);
            textSelecting=false;
            clickCount=0;
            return;
        }

        if(clickCount==2){
            selectWord(itemIndex,offset);
            textSelecting=false;
            return;
        }

        textFocus=true;
        caretStartTime=SDL_GetTicks();

        if(shift&&selectionAnchorItem>=0){
            selectionFocusItem=itemIndex;
            selectionFocusOffset=offset;
        }else{
            selectionAnchorItem=itemIndex;
            selectionAnchorOffset=offset;
            selectionFocusItem=itemIndex;
            selectionFocusOffset=offset;
        }

        textSelecting=true;
        return;
    }

    if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT){
        if(textSelecting){
            updateTextSelection();
            textSelecting=false;
            caretStartTime=SDL_GetTicks();
        }
    }
}

void Engine::updateTextSelection(){
    if(!textSelecting)return;
    int itemIndex;
    size_t offset;
    if(hitTestText(selectionMouseX,selectionMouseY,itemIndex,offset)){
        selectionFocusItem=itemIndex;
        selectionFocusOffset=offset;
        caretStartTime=SDL_GetTicks();
    }
    if(selectionMouseY<20)handleScroll(2.0f);
    else if(selectionMouseY>viewportHeight-20)handleScroll(-2.0f);
}

void Engine::renderTextSelection(){
    if(!hasSelection())return;
    int startItem,endItem;
    size_t startOffset,endOffset;
    normalizeSelection(startItem,startOffset,endItem,endOffset);
    const auto& texts=displayList.texts();
    if(startItem<0||endItem>=static_cast<int>(texts.size()))return;

    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,75,105,220,180);

    for(int i=startItem;i<=endItem;++i){
        const auto& text=texts[i];
        size_t from=i==startItem?startOffset:0;
        size_t to=i==endItem?endOffset:text.text.size();
        from=std::min(from,text.text.size());
        to=std::min(to,text.text.size());
        if(from==to)continue;
        auto bounds=textRenderer.measureRangeBounds(text,from,to);
        float h=textRenderer.lineHeight(text.size);
        SDL_FRect rect{text.x+bounds.first,text.y-scrollOffset-h*.82f,std::max(bounds.second-bounds.first,1.0f),h};
        SDL_RenderFillRect(renderer,&rect);
    }

    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}

void Engine::renderTextItem(const PaintText& text,int itemIndex){
    if(itemIndex<0||!hasSelection()){
        PaintText visible=text;
        visible.y-=scrollOffset;
        textRenderer.render(visible);
        return;
    }

    int startItem,endItem;
    size_t startOffset,endOffset;
    normalizeSelection(startItem,startOffset,endItem,endOffset);

    if(itemIndex<startItem||itemIndex>endItem){
        PaintText visible=text;
        visible.y-=scrollOffset;
        textRenderer.render(visible);
        return;
    }

    size_t from=itemIndex==startItem?startOffset:0;
    size_t to=itemIndex==endItem?endOffset:text.text.size();
    from=std::min(from,text.text.size());
    to=std::min(to,text.text.size());
    if(from>to)std::swap(from,to);

    PaintText visible=text;
    visible.y-=scrollOffset;
    textRenderer.render(visible);

    if(from<to){
        PaintText selected=text;
        selected.y-=scrollOffset;
        selected.color={255,255,255,255};
        textRenderer.renderRange(selected,from,to);
    }
}

void Engine::renderCaret(){
    if(!textFocus||textSelecting||hasSelection())return;
    if(selectionFocusItem<0||selectionFocusItem>=static_cast<int>(displayList.texts().size()))return;
    Uint64 elapsed=SDL_GetTicks()-caretStartTime;
    if((elapsed/530)%2==0)return;

    const auto& text=displayList.texts()[selectionFocusItem];
    float x=text.x+textWidth(text,selectionFocusOffset);
    float h=textRenderer.lineHeight(text.size);
    float baseline=text.y-scrollOffset;

    SDL_SetRenderDrawColor(renderer,220,220,235,255);
    SDL_FRect caret{x,baseline-h*.82f,1.5f,h};
    SDL_RenderFillRect(renderer,&caret);
}

void Engine::moveCaretHorizontal(int direction,bool extend){
    if(displayList.texts().empty())return;

    if(!extend&&hasSelection()){
        int startItem,endItem;
        size_t startOffset,endOffset;
        normalizeSelection(startItem,startOffset,endItem,endOffset);
        if(direction<0){
            selectionAnchorItem=selectionFocusItem=startItem;
            selectionAnchorOffset=selectionFocusOffset=startOffset;
        }else{
            selectionAnchorItem=selectionFocusItem=endItem;
            selectionAnchorOffset=selectionFocusOffset=endOffset;
        }
        caretStartTime=SDL_GetTicks();
        return;
    }

    if(selectionFocusItem<0){
        selectionFocusItem=0;
        selectionFocusOffset=0;
    }

    int item=selectionFocusItem;
    size_t offset=selectionFocusOffset;

    if(direction<0){
        if(offset>0)offset=previousUtf8(displayList.texts()[item].text,offset);
        else if(item>0){
            --item;
            offset=displayList.texts()[item].text.size();
        }
    }else{
        const auto& value=displayList.texts()[item].text;
        if(offset<value.size())offset=nextUtf8(value,offset);
        else if(item+1<static_cast<int>(displayList.texts().size())){
            ++item;
            offset=0;
        }
    }

    selectionFocusItem=item;
    selectionFocusOffset=offset;

    if(!extend){
        selectionAnchorItem=item;
        selectionAnchorOffset=offset;
    }

    caretStartTime=SDL_GetTicks();
}

void Engine::moveCaretVertical(int direction,bool extend){
    if(displayList.texts().empty())return;

    if(selectionFocusItem<0){
        selectionFocusItem=0;
        selectionFocusOffset=0;
    }

    int current=selectionFocusItem;
    int target=std::clamp(current+direction,0,static_cast<int>(displayList.texts().size()-1));
    const auto& currentText=displayList.texts()[current];
    float desiredX=currentText.x+textWidth(currentText,selectionFocusOffset);
    const auto& targetText=displayList.texts()[target];
    size_t offset=characterOffsetAt(targetText,desiredX-targetText.x);

    selectionFocusItem=target;
    selectionFocusOffset=offset;

    if(!extend){
        selectionAnchorItem=target;
        selectionAnchorOffset=offset;
    }

    caretStartTime=SDL_GetTicks();
}

void Engine::moveCaretToDocumentStart(bool extend){
    if(displayList.texts().empty())return;
    selectionFocusItem=0;
    selectionFocusOffset=0;
    if(!extend){
        selectionAnchorItem=0;
        selectionAnchorOffset=0;
    }
    caretStartTime=SDL_GetTicks();
}

void Engine::moveCaretToDocumentEnd(bool extend){
    if(displayList.texts().empty())return;
    int item=static_cast<int>(displayList.texts().size()-1);
    selectionFocusItem=item;
    selectionFocusOffset=displayList.texts()[item].text.size();
    if(!extend){
        selectionAnchorItem=item;
        selectionAnchorOffset=selectionFocusOffset;
    }
    caretStartTime=SDL_GetTicks();
}

void Engine::handleTextKeyboard(const SDL_Event& event){
    if(event.type!=SDL_EVENT_KEY_DOWN||!textFocus||focusedInput>=0)return;

    SDL_Keymod mods=SDL_GetModState();
    bool shift=(mods&SDL_KMOD_SHIFT)!=0;
    bool ctrl=(mods&SDL_KMOD_CTRL)!=0;

    if(ctrl&&event.key.key==SDLK_A){
        if(!displayList.texts().empty()){
            selectionAnchorItem=0;
            selectionAnchorOffset=0;
            selectionFocusItem=static_cast<int>(displayList.texts().size()-1);
            selectionFocusOffset=displayList.texts().back().text.size();
            caretStartTime=SDL_GetTicks();
        }
        return;
    }

    if(ctrl&&event.key.key==SDLK_C){
        copySelection();
        return;
    }

    switch(event.key.key){
        case SDLK_LEFT:moveCaretHorizontal(-1,shift);break;
        case SDLK_RIGHT:moveCaretHorizontal(1,shift);break;
        case SDLK_UP:moveCaretVertical(-1,shift);break;
        case SDLK_DOWN:moveCaretVertical(1,shift);break;
        case SDLK_HOME:moveCaretToDocumentStart(shift);break;
        case SDLK_END:moveCaretToDocumentEnd(shift);break;
        case SDLK_ESCAPE:clearSelection();break;
        default:return;
    }

    textSelecting=false;
}

bool Engine::handleInputMouse(const SDL_Event& event){
    if(event.type==SDL_EVENT_MOUSE_MOTION){
        mouseX=event.motion.x;
        mouseY=event.motion.y;
        hoveredButton=-1;
        bool hoveredInput=false;
        int buttonIndex=0;
        for(const auto& element:displayList.elements()){
            float y=element.y-scrollOffset;
            bool inside=mouseX>=element.x&&mouseX<=element.x+element.width&&mouseY>=y&&mouseY<=y+element.height;
            if(element.type==PaintElementType::Input&&inside)hoveredInput=true;
            if(element.type==PaintElementType::Button){
                if(inside)hoveredButton=buttonIndex;
                ++buttonIndex;
            }
        }
        if(hoveredInput)setTextCursor(true);
        else if(hoveredButton>=0)setTextCursor(false);
    }
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_LEFT){
        mouseX=event.button.x;
        mouseY=event.button.y;
        pressedButton=-1;
        int buttonIndex=0;
        for(const auto& element:displayList.elements()){
            if(element.type==PaintElementType::Button){
                float y=element.y-scrollOffset;
                if(mouseX>=element.x&&mouseX<=element.x+element.width&&mouseY>=y&&mouseY<=y+element.height){
                    pressedButton=buttonIndex;
                    break;
                }
                ++buttonIndex;
            }
        }
    }
    if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT)
        pressedButton=-1;

    const auto& elements=displayList.elements();
    auto findInput=[&](int index)->const PaintElement*{
        int current=0;
        for(const auto& element:elements)
            if(element.type==PaintElementType::Input){
                if(current++==index)return &element;
            }
        return nullptr;
    };
    auto offsetAt=[&](const PaintElement& element,const std::string& value,float mouseX){
        float localX=mouseX-(element.x+6);
        if(localX<=0)return size_t(0);
        size_t offset=0;
        while(offset<value.size()){
            size_t next=nextUtf8(value,offset);
            float left=textRenderer.measureText(value.substr(0,offset),14);
            float right=textRenderer.measureText(value.substr(0,next),14);
            if(localX<(left+right)*.5f)return offset;
            offset=next;
        }
        return value.size();
    };

    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_LEFT){
        float mouseX=event.button.x,mouseY=event.button.y;
        int inputIndex=0;
        for(const auto& element:elements){
            if(element.type!=PaintElementType::Input)continue;
            float y=element.y-scrollOffset;
            if(mouseX>=element.x&&mouseX<=element.x+element.width&&mouseY>=y&&mouseY<=y+element.height){
                Uint64 now=SDL_GetTicks();
                if(focusedInput!=inputIndex){
                    focusedInput=inputIndex;
                    inputCaret=inputValues[inputIndex].size();
                    inputClickCount=1;
                }else{
                    inputClickCount=now-lastInputClickTime<=500?inputClickCount+1:1;
                }
                lastInputClickTime=now;
                inputCaret=offsetAt(element,inputValues[inputIndex],mouseX);
                if(inputClickCount>=2){
                    inputSelectionStart=0;
                    inputSelectionEnd=inputValues[inputIndex].size();
                    inputCaret=inputSelectionEnd;
                    inputSelecting=false;
                    inputClickCount=0;
                }else{
                    inputSelectionStart=inputSelectionEnd=inputCaret;
                    inputSelecting=true;
                }
                textFocus=false;
                textSelecting=false;
                clearSelection();
                SDL_StartTextInput(window);
                setTextCursor(true);
                caretStartTime=SDL_GetTicks();
                return true;
            }
            ++inputIndex;
        }
        if(focusedInput>=0){
            focusedInput=-1;
            inputSelecting=false;
            inputClickCount=0;
            SDL_StopTextInput(window);
        }
        return false;
    }

    if(event.type==SDL_EVENT_MOUSE_MOTION){
        if(inputSelecting&&focusedInput>=0){
            const PaintElement* element=findInput(focusedInput);
            if(element){
                inputCaret=offsetAt(*element,inputValues[focusedInput],event.motion.x);
                inputSelectionEnd=inputCaret;
                caretStartTime=SDL_GetTicks();
                return true;
            }
        }
        if(focusedInput>=0){
            const PaintElement* element=findInput(focusedInput);
            if(element){
                float y=element->y-scrollOffset;
                if(event.motion.x>=element->x&&event.motion.x<=element->x+element->width&&event.motion.y>=y&&event.motion.y<=y+element->height){
                    setTextCursor(true);
                    return true;
                }
            }
        }
    }

    if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT&&inputSelecting){
        inputSelecting=false;
        return true;
    }
    return false;
}

void Engine::handleInputKeyboard(const SDL_Event& event){
    if(focusedInput<0||focusedInput>=static_cast<int>(inputValues.size())||event.type!=SDL_EVENT_KEY_DOWN)return;
    std::string& value=inputValues[focusedInput];
    SDL_Keymod mods=event.key.mod;
    bool ctrl=(mods&SDL_KMOD_CTRL)!=0;
    bool shift=(mods&SDL_KMOD_SHIFT)!=0;
    size_t start=std::min(inputSelectionStart,inputSelectionEnd);
    size_t end=std::max(inputSelectionStart,inputSelectionEnd);

    if(ctrl&&event.key.key==SDLK_A){
        inputSelectionStart=0;
        inputSelectionEnd=value.size();
        inputCaret=value.size();
        inputSelecting=false;
        caretStartTime=SDL_GetTicks();
        return;
    }
    if(ctrl&&event.key.key==SDLK_C){
        if(start!=end)SDL_SetClipboardText(value.substr(start,end-start).c_str());
        return;
    }
    if(ctrl&&event.key.key==SDLK_X){
        if(start!=end){
            SDL_SetClipboardText(value.substr(start,end-start).c_str());
            value.erase(start,end-start);
            inputCaret=start;
            inputSelectionStart=inputSelectionEnd=inputCaret;
        }
        return;
    }
    if(ctrl&&event.key.key==SDLK_V){
        char* clipboard=SDL_GetClipboardText();
        if(clipboard){
            value.replace(start,end-start,clipboard);
            inputCaret=start+std::string(clipboard).size();
            SDL_free(clipboard);
        }
        inputSelectionStart=inputSelectionEnd=inputCaret;
        caretStartTime=SDL_GetTicks();
        return;
    }

    switch(event.key.key){
        case SDLK_BACKSPACE:
            if(start!=end){
                value.erase(start,end-start);
                inputCaret=start;
            }else if(inputCaret>0){
                size_t previous=previousUtf8(value,inputCaret);
                value.erase(previous,inputCaret-previous);
                inputCaret=previous;
            }
            break;
        case SDLK_DELETE:
            if(start!=end){
                value.erase(start,end-start);
                inputCaret=start;
            }else if(inputCaret<value.size()){
                size_t next=nextUtf8(value,inputCaret);
                value.erase(inputCaret,next-inputCaret);
            }
            break;
        case SDLK_LEFT:
            inputCaret=start!=end&&!shift?start:(inputCaret>0?previousUtf8(value,inputCaret):0);
            break;
        case SDLK_RIGHT:
            inputCaret=start!=end&&!shift?end:(inputCaret<value.size()?nextUtf8(value,inputCaret):value.size());
            break;
        case SDLK_HOME:inputCaret=0;break;
        case SDLK_END:inputCaret=value.size();break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_ESCAPE:
            focusedInput=-1;
            inputSelecting=false;
            SDL_StopTextInput(window);
            return;
        default:return;
    }

    if(shift){
        inputSelectionEnd=inputCaret;
    }else{
        inputSelectionStart=inputSelectionEnd=inputCaret;
    }
    inputSelecting=false;
    caretStartTime=SDL_GetTicks();
}

void Engine::handleInputText(const SDL_Event& event){
    if(focusedInput<0||focusedInput>=static_cast<int>(inputValues.size())||event.type!=SDL_EVENT_TEXT_INPUT)return;
    std::string& value=inputValues[focusedInput];
    size_t start=std::min(inputSelectionStart,inputSelectionEnd);
    size_t end=std::max(inputSelectionStart,inputSelectionEnd);
    std::string text=event.text.text;
    value.replace(start,end-start,text);
    inputCaret=start+text.size();
    inputSelectionStart=inputSelectionEnd=inputCaret;
    caretStartTime=SDL_GetTicks();
}
void Engine::renderInputSelection(){
    if(focusedInput<0||focusedInput>=static_cast<int>(inputValues.size()))return;
    size_t start=std::min(inputSelectionStart,inputSelectionEnd);
    size_t end=std::max(inputSelectionStart,inputSelectionEnd);
    if(start==end)return;
    int current=0;
    for(const auto& element:displayList.elements()){
        if(element.type!=PaintElementType::Input)continue;
        if(current++!=focusedInput)continue;
        const std::string& value=inputValues[focusedInput];
        start=std::min(start,value.size());
        end=std::min(end,value.size());
        float x=element.x+6+textRenderer.measureText(value.substr(0,start),14);
        float width=textRenderer.measureText(value.substr(start,end-start),14);
        float y=element.y-scrollOffset;
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,75,105,220,150);
        SDL_FRect selection{x,y+3,std::max(width,1.0f),element.height-6};
        SDL_RenderFillRect(renderer,&selection);
        float textHeight=textRenderer.lineHeight(14);
        float textY=y+(element.height-textHeight)*.5f+textRenderer.baseline(14);
        PaintText selected{x,textY,14,400,value.substr(start,end-start),{255,255,255,255}};
        textRenderer.render(selected);
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
        return;
    }
}

void Engine::renderInputCaret(){
    if(focusedInput<0||focusedInput>=static_cast<int>(inputValues.size()))return;
    if(inputSelectionStart!=inputSelectionEnd)return;
    if((SDL_GetTicks()-caretStartTime)/530%2==0)return;
    int current=0;
    for(const auto& element:displayList.elements()){
        if(element.type!=PaintElementType::Input)continue;
        if(current++!=focusedInput)continue;
        const std::string& value=inputValues[focusedInput];
        size_t caret=std::min(inputCaret,value.size());
        float x=element.x+6+textRenderer.measureText(value.substr(0,caret),14);
        float y=element.y-scrollOffset;
        SDL_SetRenderDrawColor(renderer,35,35,35,255);
        SDL_FRect rect{x,y+5,1.5f,element.height-10};
        SDL_RenderFillRect(renderer,&rect);
        return;
    }
}

void Engine::rebuildLayout(int width,int height){
    if(width==static_cast<int>(viewportWidth)&&height==static_cast<int>(viewportHeight))return;
    viewportWidth=static_cast<float>(width);
    viewportHeight=static_cast<float>(height);
    if(!resizeHandler)return;
    float previousScroll=scrollOffset;
    auto previousInputs=inputValues;
    displayList=resizeHandler(viewportWidth,viewportHeight);
    inputValues.clear();
    for(const auto& element:displayList.elements()){
        if(element.type!=PaintElementType::Input)continue;
        size_t index=inputValues.size();
        inputValues.push_back(index<previousInputs.size()?previousInputs[index]:element.value);
    }
    clearSelection();
    updateContentHeight();
    scrollOffset=std::clamp(previousScroll,0.0f,std::max(0.0f,contentHeight-viewportHeight));
    targetScrollOffset=scrollOffset;
}

bool SDLCALL Engine::handleLiveResizeEvent(void* userdata,SDL_Event* event){
    if(!userdata||!event||event->type!=SDL_EVENT_WINDOW_EXPOSED||!event->window.data1)return true;
    auto* engine=static_cast<Engine*>(userdata);
    int width=0,height=0;
    if(SDL_GetWindowSize(engine->window,&width,&height)){
        engine->rebuildLayout(width,height);
        engine->renderFrame();
    }
    return true;
}

void Engine::renderFrame(){
    float cursorX=0,cursorY=0;
    SDL_GetMouseState(&cursorX,&cursorY);
    bool cursorResolved=false;
    const auto& cursorElements=displayList.elements();
    for(auto it=cursorElements.rbegin();it!=cursorElements.rend();++it){
        if(it->type!=PaintElementType::Input&&it->type!=PaintElementType::Button&&it->type!=PaintElementType::Link)continue;
        float y=it->y-scrollOffset;
        if(cursorX>=it->x&&cursorX<=it->x+it->width&&cursorY>=y&&cursorY<=y+it->height){
            if(it->type==PaintElementType::Link&&linkCursor)SDL_SetCursor(linkCursor);
            else setTextCursor(it->type==PaintElementType::Input);
            cursorResolved=true;
            break;
        }
    }
    if(!cursorResolved){
        int textIndex;
        size_t textOffset;
        setTextCursor(hitTestText(cursorX,cursorY,textIndex,textOffset));
    }
    
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    SDL_RenderClear(renderer);
    
    for(const auto& rect:displayList.rectangles()){
        float y=rect.y-scrollOffset;
        if(y+rect.height<=0||y>=viewportHeight)continue;
    
        SDL_SetRenderDrawColor(renderer,rect.color.r,rect.color.g,rect.color.b,rect.color.a);
        SDL_FRect destination{rect.x,y,rect.width,rect.height};
        SDL_RenderFillRect(renderer,&destination);
    }
    
    int inputIndex=0;
    for(const auto& element:displayList.elements()){
        int currentInput=element.type==PaintElementType::Input?inputIndex++:-1;
        float y=element.y-scrollOffset;
        if(y+element.height<=0||y>=viewportHeight)continue;
    
        SDL_FRect destination{element.x,y,element.width,element.height};
    
        if(element.type==PaintElementType::HorizontalRule){
            SDL_SetRenderDrawColor(renderer,element.color.r,element.color.g,element.color.b,element.color.a);
            SDL_RenderFillRect(renderer,&destination);
        }else if(element.type==PaintElementType::Image){
            SDL_Texture* texture=nullptr;
    
            if(!element.source.empty()){
                auto found=imageTextures.find(element.source);
                if(found==imageTextures.end()){
                    texture=IMG_LoadTexture(renderer,element.source.c_str());
                    if(texture)imageTextures.emplace(element.source,texture);
                }else{
                    texture=found->second;
                }
            }
    
            if(texture){
                SDL_RenderTexture(renderer,texture,nullptr,&destination);
            }else{
                SDL_SetRenderDrawColor(renderer,235,235,235,255);
                SDL_RenderFillRect(renderer,&destination);
                SDL_SetRenderDrawColor(renderer,150,150,150,255);
                SDL_RenderRect(renderer,&destination);
    
                if(!element.alt.empty()){
                    PaintText alt{element.x+5,y+element.height*.5f,13,400,element.alt,{70,70,70,255}};
                    textRenderer.render(alt);
                }
            }
        }else if(element.type==PaintElementType::Input){
            SDL_SetRenderDrawColor(renderer,element.color.r,element.color.g,element.color.b,element.color.a);
            SDL_RenderFillRect(renderer,&destination);
            if(currentInput>=0&&currentInput<static_cast<int>(inputValues.size())){
                const std::string& inputValue=inputValues[currentInput];
                float textHeight=textRenderer.lineHeight(14);
                float textY=y+(element.height-textHeight)*.5f+textRenderer.baseline(14);
                PaintText value{element.x+7,textY,14,400,inputValue,{35,35,35,255}};
                textRenderer.render(value);
            }
            SDL_SetRenderDrawColor(renderer,118,118,118,255);
            SDL_RenderRect(renderer,&destination);
            if(currentInput==focusedInput){
                SDL_SetRenderDrawColor(renderer,50,120,220,255);
                SDL_RenderRect(renderer,&destination);
            }
        }else if(element.type==PaintElementType::Button){
            int buttonIndex=0;
            for(const auto& candidate:displayList.elements()){
                if(candidate.type==PaintElementType::Button){
                    if(&candidate==&element)break;
                    ++buttonIndex;
                }
            }
            bool isHovered=buttonIndex==hoveredButton;
            bool isPressed=buttonIndex==pressedButton;
            Color fill=element.color;
            if(isPressed){
                fill={static_cast<uint8_t>(std::max(0,static_cast<int>(fill.r)-25)),static_cast<uint8_t>(std::max(0,static_cast<int>(fill.g)-25)),static_cast<uint8_t>(std::max(0,static_cast<int>(fill.b)-25)),fill.a};
            }else if(isHovered){
                fill={static_cast<uint8_t>(std::min(255,static_cast<int>(fill.r)+10)),static_cast<uint8_t>(std::min(255,static_cast<int>(fill.g)+10)),static_cast<uint8_t>(std::min(255,static_cast<int>(fill.b)+10)),fill.a};
            }
            SDL_SetRenderDrawColor(renderer,fill.r,fill.g,fill.b,fill.a);
            SDL_RenderFillRect(renderer,&destination);
            SDL_SetRenderDrawColor(renderer,isPressed?90:130,isPressed?90:130,isPressed?90:130,255);
            SDL_RenderRect(renderer,&destination);
            if(!element.value.empty()){
                float width=textRenderer.measureText(element.value,14);
                float textHeight=textRenderer.lineHeight(14);
                float textX=element.x+(element.width-width)*.5f;
                float textY=y+(element.height-textHeight)*.5f+textRenderer.baseline(14);
                PaintText label{textX,textY,14,400,element.value,element.textColor};
                textRenderer.render(label);
            }
        }
    }
    
    renderInputSelection();
    renderTextSelection();
    
    const auto& texts=displayList.texts();
    
    for(size_t i=0;i<texts.size();++i){
        const auto& text=texts[i];
        float h=textRenderer.lineHeight(text.size);
        float top=text.y-h*.82f-scrollOffset;
        float bottom=text.y+h*.25f-scrollOffset;
        if(bottom<=0||top>=viewportHeight)continue;
        PaintText visibleText=text;
        float pointerX=0,pointerY=0;
        SDL_GetMouseState(&pointerX,&pointerY);
        for(const auto& link:displayList.elements()){
            float linkY=link.y-scrollOffset;
            if(link.type==PaintElementType::Link&&link.textColor.a>0&&pointerX>=link.x&&pointerX<=link.x+link.width&&pointerY>=linkY&&pointerY<=linkY+link.height){
                if(text.x>=link.x&&text.x<=link.x+link.width)visibleText.color=link.textColor;
            }
        }
        renderTextItem(visibleText,static_cast<int>(i));
        if(visibleText.underline){
            float width=textRenderer.measureText(visibleText.text,visibleText.size);
            SDL_SetRenderDrawColor(renderer,visibleText.color.r,visibleText.color.g,visibleText.color.b,visibleText.color.a);
            SDL_RenderLine(renderer,visibleText.x,visibleText.y+2-scrollOffset,visibleText.x+width,visibleText.y+2-scrollOffset);
        }
    }
    
    renderCaret();
    renderInputCaret();
    renderScrollbar();
    SDL_RenderPresent(renderer);
}

void Engine::run(){
    SDL_Event event;
    Uint64 previousCounter=SDL_GetPerformanceCounter();
    double frequency=static_cast<double>(SDL_GetPerformanceFrequency());
    if(frequency<=0)frequency=1;

    SDL_AddEventWatch(handleLiveResizeEvent,this);
    while(running){
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_EVENT_QUIT)running=false;

            if(event.type==SDL_EVENT_MOUSE_WHEEL){
                float amount=event.wheel.y;
                if(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED)amount=-amount;
                handleScroll(amount);
            }

            handleInputMouse(event);
            handleInputKeyboard(event);
            handleInputText(event);

            handleTextKeyboard(event);
            handleScrollbar(event);
            handleTextMouse(event);
        }

        if(textSelecting)updateTextSelection();

        Uint64 currentCounter=SDL_GetPerformanceCounter();
        double deltaTime=static_cast<double>(currentCounter-previousCounter)/frequency;
        previousCounter=currentCounter;
        deltaTime=std::clamp(deltaTime,0.0,0.05);

        int windowWidth=0,windowHeight=0;
        if(SDL_GetWindowSize(window,&windowWidth,&windowHeight))rebuildLayout(windowWidth,windowHeight);

        float maxScroll=std::max(0.0f,contentHeight-viewportHeight);
        targetScrollOffset=std::clamp(targetScrollOffset,0.0f,maxScroll);
        scrollOffset=std::clamp(scrollOffset,0.0f,maxScroll);

        if(!draggingScrollbar){
            float smoothing=static_cast<float>(1.0-std::exp(-scrollSmoothing*deltaTime));
            scrollOffset+=(targetScrollOffset-scrollOffset)*smoothing;
            if(std::abs(targetScrollOffset-scrollOffset)<.1f)scrollOffset=targetScrollOffset;
        }

        renderFrame();
    }
    SDL_RemoveEventWatch(handleLiveResizeEvent,this);
}

void Engine::updateScrollbar(){
    if(contentHeight<=0){
        scrollbarThumbHeight=0;
        return;
    }

    scrollbarThumbHeight=std::min(
        static_cast<float>(SDL_max(35,static_cast<int>(viewportHeight*viewportHeight/contentHeight))),
        viewportHeight
    );
}

void Engine::renderScrollbar(){
    if(contentHeight<=viewportHeight||contentHeight<=0)return;

    int windowWidth=0,windowHeight=0;
    SDL_GetWindowSize(window,&windowWidth,&windowHeight);

    float trackHeight=static_cast<float>(windowHeight);

    scrollbarThumbHeight=std::max(35.0f,trackHeight*viewportHeight/contentHeight);
    scrollbarThumbHeight=std::min(scrollbarThumbHeight,trackHeight);

    float maxScroll=contentHeight-viewportHeight;
    float thumbY=(trackHeight-scrollbarThumbHeight)*(scrollOffset/maxScroll);

    SDL_SetRenderDrawColor(renderer,45,38,62,255);

    SDL_FRect track{
        static_cast<float>(windowWidth)-scrollbarWidth,
        0,
        scrollbarWidth,
        trackHeight
    };

    SDL_RenderFillRect(renderer,&track);

    if(draggingScrollbar)
        SDL_SetRenderDrawColor(renderer,210,145,255,255);
    else if(scrollbarHovered)
        SDL_SetRenderDrawColor(renderer,190,125,245,255);
    else
        SDL_SetRenderDrawColor(renderer,125,85,165,255);

    SDL_FRect thumb{
        static_cast<float>(windowWidth)-scrollbarWidth,
        thumbY,
        scrollbarWidth,
        scrollbarThumbHeight
    };

    SDL_RenderFillRect(renderer,&thumb);
}

void Engine::handleScrollbar(const SDL_Event& event){
    int windowWidth=0,windowHeight=0;
    SDL_GetWindowSize(window,&windowWidth,&windowHeight);

    float trackHeight=static_cast<float>(windowHeight);

    if(contentHeight<=viewportHeight||contentHeight<=0){
        draggingScrollbar=false;
        scrollbarHovered=false;
        return;
    }

    updateScrollbar();

    float maxScroll=contentHeight-viewportHeight;
    float maxThumbPosition=trackHeight-scrollbarThumbHeight;
    if(maxThumbPosition<=0)return;

    float thumbY=maxThumbPosition*(scrollOffset/maxScroll);

    if(event.type==SDL_EVENT_MOUSE_MOTION){
        float mouseX=event.motion.x,mouseY=event.motion.y;

        scrollbarHovered=
            mouseX>=windowWidth-scrollbarWidth&&
            mouseX<=windowWidth;

        if(draggingScrollbar){
            float thumbPosition=mouseY-scrollbarDragOffset;
            scrollOffset=std::clamp(
                thumbPosition/maxThumbPosition*maxScroll,
                0.0f,
                maxScroll
            );
            targetScrollOffset=scrollOffset;
        }
    }

    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_LEFT){
        float mouseX=event.button.x,mouseY=event.button.y;

        if(mouseX>=windowWidth-scrollbarWidth&&mouseX<=windowWidth){
            if(mouseY>=thumbY&&mouseY<=thumbY+scrollbarThumbHeight){
                draggingScrollbar=true;
                scrollbarDragOffset=mouseY-thumbY;
            }else{
                float targetPosition=mouseY-scrollbarThumbHeight/2;
                scrollOffset=std::clamp(
                    targetPosition/maxThumbPosition*maxScroll,
                    0.0f,
                    maxScroll
                );
                targetScrollOffset=scrollOffset;
                draggingScrollbar=true;
                scrollbarDragOffset=scrollbarThumbHeight/2;
            }
        }
    }

    if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT){
        draggingScrollbar=false;
        targetScrollOffset=scrollOffset;
    }
}

void Engine::shutdown(){

    for(auto& [source,texture]:imageTextures)
    if(texture)SDL_DestroyTexture(texture);
    imageTextures.clear();
    textRenderer.shutdown();

    if(arrowCursor){
        SDL_DestroyCursor(arrowCursor);
        arrowCursor=nullptr;
    }

    if(textCursor){
        SDL_DestroyCursor(textCursor);
        textCursor=nullptr;
    }
    if(linkCursor){SDL_DestroyCursor(linkCursor);linkCursor=nullptr;}

    if(renderer){
        SDL_DestroyRenderer(renderer);
        renderer=nullptr;
    }

    if(window){
        SDL_DestroyWindow(window);
        window=nullptr;
    }

    SDL_Quit();
}

}