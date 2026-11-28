#pragma once
#include <SDL3/SDL.h>
#include "render/display_list.h"
#include "render/text_renderer.h"
#include <functional>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Veltrix {

class Engine {
public:
    bool initialize();
    void setDisplayList(const DisplayList& list);
    void setDocumentBasePath(const std::string& path);
    void registerBlob(const std::string& url,const std::vector<uint8_t>& data);
    void revokeBlob(const std::string& url);
    void setResizeHandler(std::function<DisplayList(float,float)> handler);
    void getViewportSize(float& width,float& height) const;
    void run();
    void shutdown();

private:
    bool running=false;
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Cursor* arrowCursor=nullptr;
    SDL_Cursor* textCursor=nullptr;
    SDL_Cursor* linkCursor=nullptr;
    DisplayList displayList;
    TextRenderer textRenderer;

    float scrollOffset=0;
    float targetScrollOffset=0;
    float contentHeight=0;
    float viewportWidth=0;
    float viewportHeight=800;
    std::function<DisplayList(float,float)> resizeHandler;
    float scrollSpeed=48;
    float scrollSmoothing=14.0f;
    float scrollbarWidth=10;
    float scrollbarThumbHeight=0;

    bool draggingScrollbar=false;
    float dragStartY=0;
    float dragStartScroll=0;
    bool scrollbarHovered=false;
    float scrollbarDragOffset=0;

    bool textSelecting=false;
    bool textFocus=false;
    int selectionAnchorItem=-1;
    size_t selectionAnchorOffset=0;
    int selectionFocusItem=-1;
    size_t selectionFocusOffset=0;
    float selectionMouseX=0;
    float selectionMouseY=0;

    Uint64 lastClickTime=0;
    float lastClickX=0;
    float lastClickY=0;
    int clickCount=0;
    Uint64 caretStartTime=0;

    void updateScrollbar();
    void rebuildLayout(int width,int height);
    void renderFrame();
    static bool SDLCALL handleLiveResizeEvent(void* userdata,SDL_Event* event);
    void renderScrollbar();
    void handleScrollbar(const SDL_Event& event);
    void updateContentHeight();
    void handleScroll(float amount);
    void handleTextMouse(const SDL_Event& event);
    void handleTextKeyboard(const SDL_Event& event);
    void updateTextSelection();
    void renderTextSelection();

    float textWidth(const PaintText& text,size_t byteOffset);
    size_t characterOffsetAt(const PaintText& text,float localX);
    bool hitTestText(float mouseX,float mouseY,int& itemIndex,size_t& characterOffset);

    void setTextCursor(bool text);
    void clearSelection();
    bool hasSelection() const;

    void normalizeSelection(
        int& startItem,size_t& startOffset,
        int& endItem,size_t& endOffset
    ) const;

    void renderInputSelection();
    
    std::string selectedText() const;
    std::unordered_map<std::string,SDL_Texture*> imageTextures;
    std::unordered_map<std::string,std::vector<uint8_t>> blobResources;
    std::string documentBasePath;
    SDL_Texture* loadImageTexture(const std::string& source);

    void selectWord(int itemIndex,size_t offset);
    void selectLine(int itemIndex);
    void moveCaretHorizontal(int direction,bool extend);
    void moveCaretVertical(int direction,bool extend);
    void moveCaretToDocumentStart(bool extend);
    void moveCaretToDocumentEnd(bool extend);
    void copySelection();
    void renderTextItem(const PaintText& text,int itemIndex);
    void renderCaret();
    std::vector<std::string> inputValues;
    int focusedInput=-1;
    size_t inputCaret=0;
    size_t inputSelectionStart=0;
    size_t inputSelectionEnd=0;
    bool inputSelecting=false;
    Uint64 lastInputClickTime=0;
    int inputClickCount=0;
    float mouseX=0;
    float mouseY=0;
    int hoveredButton=-1;
    int pressedButton=-1;

    bool handleInputMouse(const SDL_Event& event);
    void handleInputKeyboard(const SDL_Event& event);
    void handleInputText(const SDL_Event& event);
    void renderInputCaret();
};


}