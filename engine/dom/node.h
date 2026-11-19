
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Veltrix {

enum class NodeType {
    Document,
    Element,
    Text
};

class Node {
public:
    explicit Node(NodeType type);

    NodeType type() const;

    void appendChild(std::unique_ptr<Node> child);
    const std::vector<std::unique_ptr<Node>>& children() const;

    void setName(const std::string& name);
    const std::string& name() const;

    void setText(const std::string& text);
    const std::string& text() const;

    void setAttribute(const std::string& name, const std::string& value);
    bool hasAttribute(const std::string& name) const;
    const std::string& attribute(const std::string& name) const;

    void setComputedStyle(const std::string& name, const std::string& value);
    std::string computedStyle(const std::string& name) const;

    void setCustomProperty(const std::string& name, const std::string& value);
    std::string customProperty(const std::string& name) const;
    const std::unordered_map<std::string, std::string>& customProperties() const;

private:
    NodeType nodeType;
    std::string nodeName;
    std::string nodeText;

    std::unordered_map<std::string, std::string> nodeAttributes;
    std::vector<std::unique_ptr<Node>> nodeChildren;
    std::unordered_map<std::string, std::string> computedStyles;
    std::unordered_map<std::string, std::string> customStyleProperties;
};

}
