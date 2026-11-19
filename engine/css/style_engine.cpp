
#include "style_engine.h"
#include <algorithm>
#include <cctype>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace Veltrix {

namespace {

struct Specificity {
    int ids = 0;
    int classes = 0;
    int elements = 0;

    bool operator<(const Specificity& other) const {
        return std::tie(ids, classes, elements) <
               std::tie(other.ids, other.classes, other.elements);
    }
};

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    size_t end = value.find_last_not_of(" \t\n\r");
    return value.substr(start, end - start + 1);
}

Specificity calculateSpecificity(const std::string& selector) {
    Specificity result;

    if (selector.empty()) return result;

    if (selector[0] == '#') {
        result.ids++;
    } else if (selector[0] == '.') {
        result.classes++;
    } else if (selector[0] == ':') {
        result.classes++;
    } else {
        result.elements++;
    }

    return result;
}

std::string resolveVariables(
    const std::string& value,
    const std::unordered_map<std::string, std::string>& variables,
    std::unordered_set<std::string>& resolving
) {
    std::string result;
    size_t position = 0;

    while (position < value.size()) {
        size_t start = value.find("var(", position);

        if (start == std::string::npos) {
            result += value.substr(position);
            break;
        }

        result += value.substr(position, start - position);

        size_t cursor = start + 4;
        int depth = 1;

        while (cursor < value.size() && depth > 0) {
            if (value[cursor] == '(') depth++;
            else if (value[cursor] == ')') depth--;
            cursor++;
        }

        if (depth != 0) {
            result += value.substr(start);
            break;
        }

        std::string expression = value.substr(
            start + 4,
            cursor - start - 5
        );

        size_t comma = expression.find(',');
        std::string name = trim(expression.substr(0, comma));
        std::string fallback;

        if (comma != std::string::npos) {
            fallback = trim(expression.substr(comma + 1));
        }

        std::string replacement;

        if (variables.count(name) && !resolving.count(name)) {
            resolving.insert(name);

            replacement = resolveVariables(
                variables.at(name),
                variables,
                resolving
            );

            resolving.erase(name);
        } else if (!fallback.empty()) {
            replacement = resolveVariables(
                fallback,
                variables,
                resolving
            );
        }

        result += replacement;
        position = cursor;
    }

    return result;
}

std::string resolveVariables(
    const std::string& value,
    const std::unordered_map<std::string, std::string>& variables
) {
    std::unordered_set<std::string> resolving;
    return resolveVariables(value, variables, resolving);
}

}

void StyleEngine::compute(
    Document& document,
    const std::vector<CSSRule>& rules
) {
    this->rules = rules;

    if (!document.root()) return;

    std::unordered_map<std::string, std::string> variables;
    apply(document.root(), variables);
}

void StyleEngine::apply(
    Node* node,
    const std::unordered_map<std::string, std::string>& inheritedVariables
) {
    if (!node) return;

    struct MatchedRule {
        const CSSRule* rule;
        Specificity specificity;
        size_t order;
    };

    std::vector<MatchedRule> matchedRules;

    for (size_t i = 0; i < rules.size(); i++) {
        if (matches(rules[i].selector, node)) {
            matchedRules.push_back({
                &rules[i],
                calculateSpecificity(rules[i].selector),
                i
            });
        }
    }

    std::stable_sort(
        matchedRules.begin(),
        matchedRules.end(),
        [](const MatchedRule& a, const MatchedRule& b) {
            if (a.specificity < b.specificity) return true;
            if (b.specificity < a.specificity) return false;
            return a.order < b.order;
        }
    );

    auto variables = inheritedVariables;

    for (const auto& matched : matchedRules) {
        for (const auto& [name, value] : matched.rule->style.allProperties()) {
            if (name.rfind("--", 0) == 0) {
                variables[name] = value;
            }
        }
    }

    for (const auto& [name, value] : variables) {
        node->setCustomProperty(name, value);
    }

    for (const auto& matched : matchedRules) {
        for (const auto& [name, value] : matched.rule->style.allProperties()) {
            if (name.rfind("--", 0) == 0) continue;

            std::string resolved = resolveVariables(value, variables);

            if (resolved == "inherit" ||
                resolved == "initial" ||
                resolved == "unset") {
                continue;
            }

            node->setComputedStyle(name, resolved);
        }
    }

    std::string color = node->computedStyle("color");

    if (color.empty()) {
        color = "black";
    }

    for (const auto& property : {
        "background-color",
        "border-color",
        "border-top-color",
        "border-right-color",
        "border-bottom-color",
        "border-left-color",
        "outline-color",
        "text-decoration-color",
        "fill",
        "stroke"
    }) {
        std::string value = node->computedStyle(property);

        if (value == "currentColor") {
            node->setComputedStyle(property, color);
        }
    }

    for (const auto& child : node->children()) {
        if (!child) continue;

        for (const auto& property : {
            "color",
            "font-size",
            "font-weight",
            "font-family"
        }) {
            std::string inherited = node->computedStyle(property);

            if (!inherited.empty() &&
                child->computedStyle(property).empty()) {
                child->setComputedStyle(property, inherited);
            }
        }

        apply(child.get(), variables);
    }
}

bool StyleEngine::matches(
    const std::string& selector,
    Node* node
) {
    if (!node || node->type() != NodeType::Element) return false;

    if (selector == ":root") {
        return node->name() == "html";
    }

    if (selector.empty()) return false;

    if (selector[0] == '#') {
        return node->attribute("id") == selector.substr(1);
    }

    if (selector[0] == '.') {
        std::string classes = node->attribute("class");
        std::string target = selector.substr(1);

        size_t start = 0;

        while (start < classes.size()) {
            size_t end = classes.find(' ', start);

            if (end == std::string::npos) {
                end = classes.size();
            }

            if (classes.substr(start, end - start) == target) {
                return true;
            }

            start = end + 1;
        }

        return false;
    }

    return node->name() == selector;
}

}
