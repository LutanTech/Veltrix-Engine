#pragma once

#include "../dom/document.h"
#include <string>

namespace Veltrix {

class HTMLParser {
public:
    Document parse(const std::string& html);

private:
    std::string source;
    size_t position = 0;

    void skipWhitespace();
    std::string parseTagName();
    std::string parseAttributeName();
    std::string parseAttributeValue();
    std::string parseText();
    std::unique_ptr<Node> parseElement();
};

}