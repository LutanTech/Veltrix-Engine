
#pragma once

#include "layout_node.h"

namespace Veltrix {

class LayoutEngine {
public:
    std::unique_ptr<LayoutNode> build(Node* root, float viewportWidth);

private:
    std::unique_ptr<LayoutNode> buildNode(
        Node* node,
        float availableWidth,
        float inheritedFontSize
    );

    float calculateHeight(Node* node);
    float parseDimension(const std::string& value, float fallback);
};

}
