#pragma once

#include "node.h"
#include <memory>

namespace Veltrix {

class Document {
public:
    Document();

    Node* root();
    void setRoot(std::unique_ptr<Node> root);

private:
    std::unique_ptr<Node> documentRoot;
};

}