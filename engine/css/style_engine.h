#pragma once

#include "parser.h"
#include "../dom/document.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Veltrix {

class StyleEngine {
public:
    void compute(
        Document& document,
        const std::vector<CSSRule>& rules
    );

private:
    std::vector<CSSRule> rules;

    void apply(
        Node* node,
        const std::unordered_map<std::string, std::string>& inheritedVariables,
        const std::vector<Node*>& ancestors
    );

    bool matches(const std::string& selector, Node* node);

    bool matchesSelector(
        const std::string& selector,
        Node* node,
        const std::vector<Node*>& ancestors
    );
};

}