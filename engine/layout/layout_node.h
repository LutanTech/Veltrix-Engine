
#pragma once

#include "../dom/node.h"
#include <memory>
#include <string>
#include <vector>

namespace Veltrix {

struct TextFragment {
    std::string text;
    float x = 0;
    float y = 0;
    float width = 0;
};

struct LayoutNode {
    Node* domNode = nullptr;

    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    float contentWidth = 0;
    float contentHeight = 0;

    float marginTop = 0;
    float marginRight = 0;
    float marginBottom = 0;
    float marginLeft = 0;

    float paddingTop = 0;
    float paddingRight = 0;
    float paddingBottom = 0;
    float paddingLeft = 0;

    float borderTop = 0;
    float borderRight = 0;
    float borderBottom = 0;
    float borderLeft = 0;

    float fontSize = 16;
    int fontWeight = 400;
    float lineHeight = 0;
    float fontBaseline = 0;

    std::string display = "block";

    std::vector<TextFragment> textFragments;
    std::vector<std::unique_ptr<LayoutNode>> children;
};

}
