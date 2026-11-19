#include "document.h"

namespace Veltrix {

Document::Document()
    : documentRoot(std::make_unique<Node>(NodeType::Document)) {
}

Node* Document::root() {
    return documentRoot.get();
}

void Document::setRoot(std::unique_ptr<Node> root) {
    documentRoot = std::move(root);
}

}