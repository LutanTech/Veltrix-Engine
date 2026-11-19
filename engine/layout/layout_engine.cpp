
#include "layout_engine.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace Veltrix {

std::unique_ptr<LayoutNode> LayoutEngine::build(
    Node* root,
    float viewportWidth
) {
    if (!root) return nullptr;

    return buildNode(root, viewportWidth, 16);
}

std::unique_ptr<LayoutNode> LayoutEngine::buildNode(
    Node* node,
    float availableWidth,
    float inheritedFontSize
) {
    if (!node) return nullptr;

    auto layout = std::make_unique<LayoutNode>();

    layout->domNode = node;
    layout->width = availableWidth;
    layout->fontSize = inheritedFontSize;

    if (node->type() == NodeType::Text) {
        layout->height = std::max(20.0f, inheritedFontSize * 1.2f);
        return layout;
    }

    if (node->type() == NodeType::Element) {
        std::string tag = node->name();

        std::transform(
            tag.begin(),
            tag.end(),
            tag.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        if (
            tag == "head" ||
            tag == "style" ||
            tag == "script" ||
            tag == "title" ||
            tag == "meta" ||
            tag == "link"
        ) {
            return layout;
        }

        layout->width = parseDimension(
            node->computedStyle("width"),
            availableWidth
        );

        layout->fontSize = parseDimension(
            node->computedStyle("font-size"),
            inheritedFontSize
        );

        layout->height = parseDimension(
            node->computedStyle("height"),
            0
        );
    }

    float currentY = 0;

    for (const auto& child : node->children()) {
        auto childLayout = buildNode(
            child.get(),
            layout->width,
            layout->fontSize
        );

        if (!childLayout) continue;

        childLayout->x = 0;
        childLayout->y = currentY;

        currentY += childLayout->height;

        layout->children.push_back(std::move(childLayout));
    }

    if (layout->height == 0) {
        layout->height = std::max(
            currentY,
            calculateHeight(node)
        );
    }

    return layout;
}

float LayoutEngine::calculateHeight(Node* node) {
    if (!node || node->type() != NodeType::Element) return 0;

    if (node->name() == "h1") return 38;
    if (node->name() == "h2") return 32;
    if (node->name() == "h3") return 28;
    if (node->name() == "p") return 24;
    if (node->name() == "img") return 150;

    return 0;
}

float LayoutEngine::parseDimension(
    const std::string& value,
    float fallback
) {
    if (value.empty() || value == "auto") return fallback;

    char* end = nullptr;
    float result = std::strtof(value.c_str(), &end);

    if (end == value.c_str()) return fallback;

    return result;
}

}
