
#pragma once

#include "../dom/node.h"
#include <memory>
#include <vector>

namespace Veltrix {

struct LayoutNode {
    Node* domNode = nullptr;

    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    float fontSize = 16;

    std::vector<std::unique_ptr<LayoutNode>> children;
};

}
