#include "style_engine.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
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

struct SelectorPart {
    std::string selector;
    char combinator = ' ';
};

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\n\r");

    if (start == std::string::npos) {
        return "";
    }

    size_t end = value.find_last_not_of(" \t\n\r");

    return value.substr(start, end - start + 1);
}

bool isWhitespace(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

std::vector<std::string> splitSelectorList(const std::string& selector) {
    std::vector<std::string> result;
    std::string current;
    int bracketDepth = 0;
    int parenthesisDepth = 0;

    for (char c : selector) {
        if (c == '[') {
            bracketDepth++;
        } else if (c == ']') {
            bracketDepth--;
        } else if (c == '(') {
            parenthesisDepth++;
        } else if (c == ')') {
            parenthesisDepth--;
        }

        if (c == ',' &&
            bracketDepth == 0 &&
            parenthesisDepth == 0) {
            std::string value = trim(current);

            if (!value.empty()) {
                result.push_back(value);
            }

            current.clear();
        } else {
            current += c;
        }
    }

    std::string value = trim(current);

    if (!value.empty()) {
        result.push_back(value);
    }

    return result;
}

std::vector<SelectorPart> parseSelector(const std::string& selector) {
    std::vector<SelectorPart> result;
    std::string current;
    char pendingCombinator = ' ';
    bool hadWhitespace = false;
    int bracketDepth = 0;
    int parenthesisDepth = 0;

    auto pushCurrent = [&]() {
        std::string value = trim(current);

        if (!value.empty()) {
            result.push_back({
                value,
                pendingCombinator
            });

            current.clear();
            pendingCombinator = ' ';
        }
    };

    for (size_t i = 0; i < selector.size(); i++) {
        char c = selector[i];

        if (c == '[') {
            bracketDepth++;
            current += c;
            hadWhitespace = false;
            continue;
        }

        if (c == ']') {
            bracketDepth--;
            current += c;
            hadWhitespace = false;
            continue;
        }

        if (c == '(') {
            parenthesisDepth++;
            current += c;
            hadWhitespace = false;
            continue;
        }

        if (c == ')') {
            parenthesisDepth--;
            current += c;
            hadWhitespace = false;
            continue;
        }

        if (bracketDepth == 0 &&
            parenthesisDepth == 0 &&
            isWhitespace(c)) {
            if (!current.empty()) {
                pushCurrent();
            }

            hadWhitespace = true;
            continue;
        }

        if (bracketDepth == 0 &&
            parenthesisDepth == 0 &&
            (c == '>' || c == '+' || c == '~')) {
            if (!current.empty()) {
                pushCurrent();
            }

            pendingCombinator = c;
            hadWhitespace = false;
            continue;
        }

        if (hadWhitespace &&
            !current.empty()) {
            pushCurrent();
        }

        current += c;
        hadWhitespace = false;
    }

    pushCurrent();

    return result;
}

bool hasClass(Node* node, const std::string& target) {
    if (!node) {
        return false;
    }

    std::string classes = node->attribute("class");
    size_t start = 0;

    while (start < classes.size()) {
        while (start < classes.size() &&
               isWhitespace(classes[start])) {
            start++;
        }

        if (start >= classes.size()) {
            break;
        }

        size_t end = start;

        while (end < classes.size() &&
               !isWhitespace(classes[end])) {
            end++;
        }

        if (classes.substr(start, end - start) == target) {
            return true;
        }

        start = end;
    }

    return false;
}

std::string unquote(const std::string& value) {
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

bool attributeMatches(
    Node* node,
    const std::string& expression
) {
    if (!node) {
        return false;
    }

    std::string value = trim(expression);

    static const std::vector<std::string> operators = {
        "~=",
        "|=",
        "^=",
        "$=",
        "*=",
        "="
    };

    for (const auto& op : operators) {
        size_t position = value.find(op);

        if (position == std::string::npos) {
            continue;
        }

        std::string name = trim(value.substr(0, position));
        std::string expected = unquote(
            trim(value.substr(position + op.size()))
        );

        if (name.empty()) {
            return false;
        }

        std::string actual = node->attribute(name);

        if (op == "=") {
            return actual == expected;
        }

        if (op == "~=") {
            size_t start = 0;

            while (start < actual.size()) {
                while (start < actual.size() &&
                       isWhitespace(actual[start])) {
                    start++;
                }

                size_t end = start;

                while (end < actual.size() &&
                       !isWhitespace(actual[end])) {
                    end++;
                }

                if (actual.substr(start, end - start) == expected) {
                    return true;
                }

                start = end;
            }

            return false;
        }

        if (op == "|=") {
            return actual == expected ||
                   (actual.size() > expected.size() &&
                    actual.rfind(expected + "-", 0) == 0);
        }

        if (op == "^=") {
            return actual.rfind(expected, 0) == 0;
        }

        if (op == "$=") {
            if (expected.size() > actual.size()) {
                return false;
            }

            return actual.compare(
                actual.size() - expected.size(),
                expected.size(),
                expected
            ) == 0;
        }

        if (op == "*=") {
            return actual.find(expected) != std::string::npos;
        }
    }

    return node->hasAttribute(value);
}

std::vector<std::string> splitCompound(const std::string& selector) {
    std::vector<std::string> result;
    std::string current;

    for (size_t i = 0; i < selector.size();) {
        char c = selector[i];

        if (c == '#') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }

            size_t start = i++;

            while (i < selector.size() &&
                   selector[i] != '#' &&
                   selector[i] != '.' &&
                   selector[i] != '[' &&
                   selector[i] != ':') {
                i++;
            }

            result.push_back(selector.substr(start, i - start));
            continue;
        }

        if (c == '.') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }

            size_t start = i++;

            while (i < selector.size() &&
                   selector[i] != '#' &&
                   selector[i] != '.' &&
                   selector[i] != '[' &&
                   selector[i] != ':') {
                i++;
            }

            result.push_back(selector.substr(start, i - start));
            continue;
        }

        if (c == '[') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }

            size_t start = i++;
            int depth = 1;

            while (i < selector.size() && depth > 0) {
                if (selector[i] == '[') {
                    depth++;
                } else if (selector[i] == ']') {
                    depth--;
                }

                i++;
            }

            result.push_back(selector.substr(start, i - start));
            continue;
        }

        if (c == ':') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }

            size_t start = i++;

            while (i < selector.size() &&
                   selector[i] != '#' &&
                   selector[i] != '.' &&
                   selector[i] != '[' &&
                   selector[i] != ':') {
                i++;
            }

            result.push_back(selector.substr(start, i - start));
            continue;
        }

        current += c;
        i++;
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

int elementIndex(
    Node* node,
    const std::vector<Node*>& siblings
) {
    int index = 0;

    for (Node* sibling : siblings) {
        if (!sibling ||
            sibling->type() != NodeType::Element) {
            continue;
        }

        index++;

        if (sibling == node) {
            return index;
        }
    }

    return 0;
}

int elementTypeIndex(
    Node* node,
    const std::vector<Node*>& siblings
) {
    int index = 0;

    for (Node* sibling : siblings) {
        if (!sibling ||
            sibling->type() != NodeType::Element) {
            continue;
        }

        if (sibling->name() != node->name()) {
            continue;
        }

        index++;

        if (sibling == node) {
            return index;
        }
    }

    return 0;
}

int elementCount(
    const std::vector<Node*>& siblings
) {
    int count = 0;

    for (Node* sibling : siblings) {
        if (sibling &&
            sibling->type() == NodeType::Element) {
            count++;
        }
    }

    return count;
}

bool nthMatches(int index, const std::string& expression) {
    std::string value = trim(expression);

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    if (value == "odd") {
        return index % 2 == 1;
    }

    if (value == "even") {
        return index % 2 == 0;
    }

    char* end = nullptr;
    long number = std::strtol(
        value.c_str(),
        &end,
        10
    );

    if (end != value.c_str() &&
        *end == '\0') {
        return index == number;
    }

    size_t n = value.find('n');

    if (n == std::string::npos) {
        return false;
    }

    std::string a = trim(value.substr(0, n));
    std::string b = trim(value.substr(n + 1));

    int coefficient = 1;

    if (a == "-") {
        coefficient = -1;
    } else if (!a.empty() &&
               a != "+") {
        coefficient = std::atoi(a.c_str());
    }

    int offset = 0;

    if (!b.empty()) {
        offset = std::atoi(b.c_str());
    }

    if (coefficient == 0) {
        return index == offset;
    }

    int difference = index - offset;

    if (coefficient > 0) {
        return difference >= 0 &&
               difference % coefficient == 0;
    }

    return difference <= 0 &&
           difference % coefficient == 0;
}

bool matchesPseudo(
    const std::string& pseudo,
    Node* node,
    const std::vector<Node*>& ancestors
);

bool matchesCompound(
    const std::string& selector,
    Node* node,
    const std::vector<Node*>& ancestors
) {
    if (!node ||
        node->type() != NodeType::Element) {
        return false;
    }

    if (selector.empty()) {
        return false;
    }

    auto parts = splitCompound(selector);

    for (const auto& part : parts) {
        if (part.empty()) {
            continue;
        }

        if (part == "*") {
            continue;
        }

        if (part[0] == '#') {
            if (node->attribute("id") != part.substr(1)) {
                return false;
            }

            continue;
        }

        if (part[0] == '.') {
            if (!hasClass(node, part.substr(1))) {
                return false;
            }

            continue;
        }

        if (part[0] == '[') {
            if (part.back() != ']') {
                return false;
            }

            if (!attributeMatches(
                node,
                part.substr(1, part.size() - 2)
            )) {
                return false;
            }

            continue;
        }

        if (part[0] == ':') {
            if (!matchesPseudo(
                part,
                node,
                ancestors
            )) {
                return false;
            }

            continue;
        }

        if (node->name() != part) {
            return false;
        }
    }

    return true;
}

bool matchesPseudo(
    const std::string& pseudo,
    Node* node,
    const std::vector<Node*>& ancestors
) {
    std::string name = pseudo;
    std::string argument;

    size_t open = pseudo.find('(');

    if (open != std::string::npos &&
        pseudo.back() == ')') {
        name = pseudo.substr(0, open);
        argument = pseudo.substr(
            open + 1,
            pseudo.size() - open - 2
        );
    }

    if (name == ":root") {
        return node->name() == "html";
    }

    if (name == ":first-child" ||
        name == ":last-child" ||
        name == ":only-child" ||
        name == ":nth-child") {
        if (ancestors.empty()) {
            return false;
        }

        Node* parent = ancestors.back();

        std::vector<Node*> siblings;

        for (const auto& child : parent->children()) {
            if (child) {
                siblings.push_back(child.get());
            }
        }

        int index = elementIndex(
            node,
            siblings
        );

        int count = elementCount(siblings);

        if (name == ":first-child") {
            return index == 1;
        }

        if (name == ":last-child") {
            return index == count;
        }

        if (name == ":only-child") {
            return count == 1;
        }

        return nthMatches(
            index,
            argument
        );
    }

    if (name == ":first-of-type" ||
        name == ":last-of-type" ||
        name == ":only-of-type" ||
        name == ":nth-of-type") {
        if (ancestors.empty()) {
            return false;
        }

        Node* parent = ancestors.back();

        std::vector<Node*> siblings;

        for (const auto& child : parent->children()) {
            if (child) {
                siblings.push_back(child.get());
            }
        }

        int index = elementTypeIndex(
            node,
            siblings
        );

        int count = 0;

        for (Node* sibling : siblings) {
            if (sibling &&
                sibling->type() == NodeType::Element &&
                sibling->name() == node->name()) {
                count++;
            }
        }

        if (name == ":first-of-type") {
            return index == 1;
        }

        if (name == ":last-of-type") {
            return index == count;
        }

        if (name == ":only-of-type") {
            return count == 1;
        }

        return nthMatches(
            index,
            argument
        );
    }

    if (name == ":empty") {
        return node->children().empty();
    }

    if (name == ":not") {
        for (const auto& selector : splitSelectorList(argument)) {
            if (matchesCompound(
                selector,
                node,
                ancestors
            )) {
                return false;
            }
        }

        return true;
    }

    if (name == ":is" ||
        name == ":where") {
        for (const auto& selector : splitSelectorList(argument)) {
            auto parts = parseSelector(selector);

            if (parts.size() == 1 &&
                matchesCompound(
                    parts[0].selector,
                    node,
                    ancestors
                )) {
                return true;
            }
        }

        return false;
    }

    if (name == ":hover" ||
        name == ":active" ||
        name == ":focus" ||
        name == ":visited" ||
        name == ":link" ||
        name == ":focus-within" ||
        name == ":focus-visible") {
        return false;
    }

    return false;
}

bool previousElementSibling(
    Node* node,
    const std::vector<Node*>& ancestors,
    Node*& result
) {
    result = nullptr;

    if (ancestors.empty()) {
        return false;
    }

    Node* parent = ancestors.back();

    Node* previous = nullptr;

    for (const auto& child : parent->children()) {
        if (!child) {
            continue;
        }

        Node* current = child.get();

        if (current == node) {
            result = previous;
            return result != nullptr;
        }

        if (current->type() == NodeType::Element) {
            previous = current;
        }
    }

    return false;
}

bool matchesSelectorParts(
    const std::vector<SelectorPart>& parts,
    int index,
    Node* node,
    const std::vector<Node*>& ancestors
) {
    if (index < 0 ||
        !node) {
        return false;
    }

    if (!matchesCompound(
        parts[index].selector,
        node,
        ancestors
    )) {
        return false;
    }

    if (index == 0) {
        return true;
    }

    char combinator = parts[index].combinator;

    if (combinator == '>') {
        if (ancestors.empty()) {
            return false;
        }

        std::vector<Node*> parentAncestors = ancestors;

        Node* parent = parentAncestors.back();
        parentAncestors.pop_back();

        return matchesSelectorParts(
            parts,
            index - 1,
            parent,
            parentAncestors
        );
    }

    if (combinator == '+') {
        Node* previous = nullptr;

        if (!previousElementSibling(
            node,
            ancestors,
            previous
        )) {
            return false;
        }

        std::vector<Node*> previousAncestors = ancestors;

        if (!previousAncestors.empty()) {
            previousAncestors.pop_back();
        }

        return matchesSelectorParts(
            parts,
            index - 1,
            previous,
            previousAncestors
        );
    }

    if (combinator == '~') {
        if (ancestors.empty()) {
            return false;
        }

        Node* parent = ancestors.back();
        Node* previous = nullptr;

        std::vector<Node*> previousAncestors = ancestors;

        previousAncestors.pop_back();

        for (const auto& child : parent->children()) {
            if (!child) {
                continue;
            }

            Node* current = child.get();

            if (current == node) {
                break;
            }

            if (current->type() != NodeType::Element) {
                continue;
            }

            previous = current;

            if (matchesSelectorParts(
                parts,
                index - 1,
                previous,
                previousAncestors
            )) {
                return true;
            }
        }

        return false;
    }

    for (int i = static_cast<int>(ancestors.size()) - 1;
         i >= 0;
         i--) {
        std::vector<Node*> ancestorList(
            ancestors.begin(),
            ancestors.begin() + i
        );

        if (matchesSelectorParts(
            parts,
            index - 1,
            ancestors[i],
            ancestorList
        )) {
            return true;
        }
    }

    return false;
}

bool matchesSingleSelector(
    const std::string& selector,
    Node* node,
    const std::vector<Node*>& ancestors
) {
    auto parts = parseSelector(selector);

    if (parts.empty()) {
        return false;
    }

    return matchesSelectorParts(
        parts,
        static_cast<int>(parts.size()) - 1,
        node,
        ancestors
    );
}

Specificity calculateSingleSpecificity(
    const std::string& selector
) {
    Specificity result;

    auto parts = parseSelector(selector);

    for (const auto& part : parts) {
        auto compound = splitCompound(part.selector);

        for (const auto& item : compound) {
            if (item.empty() || item == "*") {
                continue;
            }

            if (item[0] == '#') {
                result.ids++;
                continue;
            }

            if (item[0] == '.') {
                result.classes++;
                continue;
            }

            if (item[0] == '[') {
                result.classes++;
                continue;
            }

            if (item[0] == ':') {
                if (item.rfind(":where(", 0) == 0) {
                    continue;
                }

                if (item.rfind(":not(", 0) == 0 ||
                    item.rfind(":is(", 0) == 0) {
                    std::string argument = item.substr(
                        item.find('(') + 1,
                        item.size() - item.find('(') - 2
                    );

                    Specificity best;

                    for (const auto& option : splitSelectorList(argument)) {
                        Specificity current =
                            calculateSingleSpecificity(option);

                        if (best < current) {
                            best = current;
                        }
                    }

                    result.ids += best.ids;
                    result.classes += best.classes;
                    result.elements += best.elements;
                } else {
                    result.classes++;
                }

                continue;
            }

            result.elements++;
        }
    }

    return result;
}

Specificity calculateSpecificity(
    const std::string& selector
) {
    Specificity best;

    for (const auto& option : splitSelectorList(selector)) {
        Specificity current =
            calculateSingleSpecificity(option);

        if (best < current) {
            best = current;
        }
    }

    return best;
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

        result += value.substr(
            position,
            start - position
        );

        size_t cursor = start + 4;
        int depth = 1;

        while (cursor < value.size() && depth > 0) {
            if (value[cursor] == '(') {
                depth++;
            } else if (value[cursor] == ')') {
                depth--;
            }

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
        std::string name = trim(
            expression.substr(0, comma)
        );

        std::string fallback;

        if (comma != std::string::npos) {
            fallback = trim(
                expression.substr(comma + 1)
            );
        }

        std::string replacement;

        if (variables.count(name) &&
            !resolving.count(name)) {
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

    return resolveVariables(
        value,
        variables,
        resolving
    );
}

}

void StyleEngine::compute(
    Document& document,
    const std::vector<CSSRule>& rules
) {
    this->rules = rules;

    if (!document.root()) {
        return;
    }

    std::unordered_map<std::string, std::string> variables;
    std::vector<Node*> ancestors;

    apply(
        document.root(),
        variables,
        ancestors
    );
}

void StyleEngine::apply(
    Node* node,
    const std::unordered_map<std::string, std::string>& inheritedVariables,
    const std::vector<Node*>& ancestors
) {
    if (!node) {
        return;
    }

    struct MatchedRule {
        const CSSRule* rule;
        Specificity specificity;
        size_t order;
    };

    std::vector<MatchedRule> matchedRules;

    for (size_t i = 0; i < rules.size(); i++) {
        if (matchesSelector(
            rules[i].selector,
            node,
            ancestors
        )) {
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
            if (a.specificity < b.specificity) {
                return true;
            }

            if (b.specificity < a.specificity) {
                return false;
            }

            return a.order < b.order;
        }
    );

    auto variables = inheritedVariables;

    for (const auto& matched : matchedRules) {
        for (const auto& [name, value] :
             matched.rule->style.allProperties()) {
            if (name.rfind("--", 0) == 0) {
                variables[name] = value;
            }
        }
    }

    for (const auto& [name, value] : variables) {
        node->setCustomProperty(
            name,
            value
        );
    }

    for (const auto& matched : matchedRules) {
        for (const auto& [name, value] :
             matched.rule->style.allProperties()) {
            if (name.rfind("--", 0) == 0) {
                continue;
            }

            std::string resolved =
                resolveVariables(
                    value,
                    variables
                );

            if (resolved == "inherit" ||
                resolved == "initial" ||
                resolved == "unset") {
                continue;
            }

            node->setComputedStyle(
                name,
                resolved
            );
        }
    }

    std::string color =
        node->computedStyle("color");

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
        std::string value =
            node->computedStyle(property);

        if (value == "currentColor") {
            node->setComputedStyle(
                property,
                color
            );
        }
    }

    for (const auto& child : node->children()) {
        if (!child) {
            continue;
        }

        for (const auto& property : {
            "color",
            "font-size",
            "font-weight",
            "font-family"
        }) {
            std::string inherited =
                node->computedStyle(property);

            if (!inherited.empty() &&
                child->computedStyle(property).empty()) {
                child->setComputedStyle(
                    property,
                    inherited
                );
            }
        }

        auto childAncestors = ancestors;
        childAncestors.push_back(node);

        apply(
            child.get(),
            variables,
            childAncestors
        );
    }
}

bool StyleEngine::matches(
    const std::string& selector,
    Node* node
) {
    if (!node ||
        node->type() != NodeType::Element) {
        return false;
    }

    std::vector<Node*> ancestors;

    return matchesSelector(
        selector,
        node,
        ancestors
    );
}

bool StyleEngine::matchesSelector(
    const std::string& selector,
    Node* node,
    const std::vector<Node*>& ancestors
) {
    for (const auto& option :
         splitSelectorList(selector)) {
        if (matchesSingleSelector(
            option,
            node,
            ancestors
        )) {
            return true;
        }
    }

    return false;
}

}