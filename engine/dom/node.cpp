
#include "node.h"

namespace Veltrix {

Node::Node(NodeType type)
    : nodeType(type) {
}

NodeType Node::type() const {
    return nodeType;
}

void Node::appendChild(std::unique_ptr<Node> child) {
    nodeChildren.push_back(std::move(child));
}

const std::vector<std::unique_ptr<Node>>& Node::children() const {
    return nodeChildren;
}

void Node::setName(const std::string& name) {
    nodeName = name;
}

const std::string& Node::name() const {
    return nodeName;
}

void Node::setText(const std::string& text) {
    nodeText = text;
}

const std::string& Node::text() const {
    return nodeText;
}

void Node::setAttribute(const std::string& name, const std::string& value) {
    nodeAttributes[name] = value;
}

bool Node::hasAttribute(const std::string& name) const {
    return nodeAttributes.contains(name);
}

const std::string& Node::attribute(const std::string& name) const {
    static const std::string empty;

    auto it = nodeAttributes.find(name);

    return it == nodeAttributes.end() ? empty : it->second;
}

void Node::setComputedStyle(const std::string& name, const std::string& value) {
    computedStyles[name] = value;
}

std::string Node::computedStyle(const std::string& name) const {
    auto it = computedStyles.find(name);

    return it == computedStyles.end() ? "" : it->second;
}

void Node::setCustomProperty(const std::string& name, const std::string& value) {
    customStyleProperties[name] = value;
}

std::string Node::customProperty(const std::string& name) const {
    auto it = customStyleProperties.find(name);

    return it == customStyleProperties.end() ? "" : it->second;
}

const std::unordered_map<std::string, std::string>& Node::customProperties() const {
    return customStyleProperties;
}

}
