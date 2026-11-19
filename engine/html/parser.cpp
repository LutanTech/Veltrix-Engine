#include "parser.h"
#include <cctype>

namespace Veltrix {

Document HTMLParser::parse(const std::string& html) {
    source = html;
    position = 0;

    Document document;

    while (position < source.size()) {
        skipWhitespace();

        if (position >= source.size()) {
            break;
        }

        if (source[position] == '<') {
            document.setRoot(parseElement());
            break;
        }

        ++position;
    }

    return document;
}

void HTMLParser::skipWhitespace() {
    while (position < source.size() &&
           std::isspace(static_cast<unsigned char>(source[position]))) {
        ++position;
    }
}

std::string HTMLParser::parseTagName() {
    std::string name;

    while (position < source.size() &&
           (std::isalnum(static_cast<unsigned char>(source[position])) ||
            source[position] == '-' ||
            source[position] == '_')) {
        name += source[position++];
    }

    return name;
}

std::string HTMLParser::parseAttributeName() {
    std::string name;

    while (position < source.size() &&
           (std::isalnum(static_cast<unsigned char>(source[position])) ||
            source[position] == '-' ||
            source[position] == '_')) {
        name += source[position++];
    }

    return name;
}

std::string HTMLParser::parseAttributeValue() {
    skipWhitespace();

    if (position >= source.size()) {
        return "";
    }

    if (source[position] == '"' || source[position] == '\'') {
        char quote = source[position++];
        std::string value;

        while (position < source.size() && source[position] != quote) {
            value += source[position++];
        }

        if (position < source.size()) {
            ++position;
        }

        return value;
    }

    std::string value;

    while (position < source.size() &&
           !std::isspace(static_cast<unsigned char>(source[position])) &&
           source[position] != '>') {
        value += source[position++];
    }

    return value;
}

std::string HTMLParser::parseText() {
    std::string text;

    while (position < source.size() && source[position] != '<') {
        text += source[position++];
    }

    return text;
}

std::unique_ptr<Node> HTMLParser::parseElement() {
    ++position;

    std::string name = parseTagName();

    auto element = std::make_unique<Node>(NodeType::Element);
    element->setName(name);

    while (position < source.size() && source[position] != '>') {
        skipWhitespace();

        if (position >= source.size() || source[position] == '>') {
            break;
        }

        if (source[position] == '/') {
            ++position;
            continue;
        }

        std::string attributeName = parseAttributeName();

        if (attributeName.empty()) {
            ++position;
            continue;
        }

        skipWhitespace();

        std::string attributeValue;

        if (position < source.size() && source[position] == '=') {
            ++position;
            attributeValue = parseAttributeValue();
        }

        element->setAttribute(attributeName, attributeValue);
    }

    if (position < source.size() && source[position] == '>') {
        ++position;
    }

    while (position < source.size()) {
        if (source[position] == '<' &&
            position + 1 < source.size() &&
            source[position + 1] == '/') {

            position += 2;

            parseTagName();

            while (position < source.size() && source[position] != '>') {
                ++position;
            }

            if (position < source.size()) {
                ++position;
            }

            break;
        }

        if (source[position] == '<') {
            element->appendChild(parseElement());
        } else {
            std::string text = parseText();

            if (!text.empty()) {
                auto textNode = std::make_unique<Node>(NodeType::Text);
                textNode->setText(text);
                element->appendChild(std::move(textNode));
            }
        }
    }

    return element;
}

}