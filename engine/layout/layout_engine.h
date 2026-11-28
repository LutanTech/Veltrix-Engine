
#pragma once

#include "layout_node.h"
#include "../render/font_manager.h"
#include <memory>
#include <string>

namespace Veltrix {

class LayoutEngine {
public:
    bool initialize(const std::string& fontPath);
    void shutdown();

    std::unique_ptr<LayoutNode> build(
        Node* root,
        float viewportWidth
    );

private:
    FontManager fontManager;
    bool fontReady = false;
    bool fontAttempted = false;

    std::unique_ptr<LayoutNode> buildNode(
        Node* node,
        float availableWidth,
        float inheritedFontSize,
        int inheritedFontWeight
    );

    float calculateHeight(Node* node);

    float parseDimension(
        const std::string& value,
        float fallback
    );

    float measureText(
        const std::string& text,
        float fontSize,
        int fontWeight = 400
    );

    void layoutText(
        LayoutNode& layout,
        const std::string& text,
        float availableWidth
    );
};

}
