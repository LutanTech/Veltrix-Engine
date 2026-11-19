#pragma once

#include "display_list.h"
#include "../layout/layout_node.h"
#include <string>

namespace Veltrix {

class DisplayListBuilder {
public:
    DisplayList build(const LayoutNode* root);

private:
    void buildNode(
        const LayoutNode* node,
        DisplayList& displayList,
        float parentX,
        float parentY,
        const std::string& inheritedColor
    );

    Color parseColor(
        const std::string& value,
        const std::string& currentColor = "black"
    );
};

}