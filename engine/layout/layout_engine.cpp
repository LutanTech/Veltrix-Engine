
#include "layout_engine.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace Veltrix {

namespace {

float parseNumber(
    const std::string& value,
    float fallback = 0
) {
    if (value.empty() || value == "auto") {
        return fallback;
    }

    char* end = nullptr;
    float result = std::strtof(value.c_str(), &end);

    if (end == value.c_str()) {
        return fallback;
    }

    return result;
}

std::string lower(const std::string& value) {
    std::string result = value;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    return result;
}

std::vector<float> parseBoxValues(const std::string& value) {
    std::vector<float> values;
    std::string current;

    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                values.push_back(parseNumber(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        values.push_back(parseNumber(current));
    }

    return values;
}

void expandBox(
    const std::string& value,
    float& top,
    float& right,
    float& bottom,
    float& left
) {
    auto values = parseBoxValues(value);

    if (values.empty()) {
        top = right = bottom = left = 0;
    } else if (values.size() == 1) {
        top = right = bottom = left = values[0];
    } else if (values.size() == 2) {
        top = bottom = values[0];
        right = left = values[1];
    } else if (values.size() == 3) {
        top = values[0];
        right = left = values[1];
        bottom = values[2];
    } else {
        top = values[0];
        right = values[1];
        bottom = values[2];
        left = values[3];
    }
}

void parseBox(
    Node* node,
    const char* property,
    float& top,
    float& right,
    float& bottom,
    float& left
) {
    if (!node) return;

    std::string value = node->computedStyle(property);

    if (!value.empty()) {
        expandBox(value, top, right, bottom, left);
    }
}

void parseBorders(
    Node* node,
    float& top,
    float& right,
    float& bottom,
    float& left
) {
    if (!node) return;

    std::string borderWidth = node->computedStyle("border-width");

    if (!borderWidth.empty()) {
        expandBox(borderWidth, top, right, bottom, left);
    }

    std::string value;

    value = node->computedStyle("border-top-width");
    if (!value.empty()) top = parseNumber(value, top);

    value = node->computedStyle("border-right-width");
    if (!value.empty()) right = parseNumber(value, right);

    value = node->computedStyle("border-bottom-width");
    if (!value.empty()) bottom = parseNumber(value, bottom);

    value = node->computedStyle("border-left-width");
    if (!value.empty()) left = parseNumber(value, left);

    std::string shorthand = node->computedStyle("border");

    if (!shorthand.empty() && borderWidth.empty()) {
        size_t position = shorthand.find_first_of("0123456789.");

        if (position != std::string::npos) {
            float width = parseNumber(shorthand.substr(position), 0);
            top = right = bottom = left = width;
        }
    }
}

std::string defaultDisplay(const std::string& tag) {
    static const std::vector<std::string> inlineTags = {
        "a", "abbr", "b", "bdi", "bdo",
        "br", "cite", "code", "del", "em",
        "i", "img", "input", "ins", "kbd",
        "label", "mark", "q", "s", "samp",
        "select", "small", "span", "strong",
        "sub", "sup", "textarea", "time",
        "u", "var", "button"
    };

    if (std::find(
        inlineTags.begin(),
        inlineTags.end(),
        tag
    ) != inlineTags.end()) {
        return "inline";
    }

    return "block";
}

bool isInlineDisplay(const std::string& display) {
    return display == "inline" ||
           display == "inline-block";
}

bool isHiddenTag(const std::string& tag) {
    return tag == "head" ||
           tag == "style" ||
           tag == "script" ||
           tag == "title" ||
           tag == "meta" ||
           tag == "link";
}

size_t utf8Length(
    const std::string& text,
    size_t position
) {
    unsigned char c = static_cast<unsigned char>(text[position]);

    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;

    return 1;
}

float fallbackTextWidth(
    const std::string& text,
    float fontSize
) {
    float width = 0;

    for (unsigned char c : text) {
        if (c == ' ' || c == '\t') {
            width += fontSize * 0.28f;
        } else {
            width += fontSize * 0.55f;
        }
    }

    return width;
}

}

bool LayoutEngine::initialize(const std::string& fontPath) {
    fontAttempted = true;
    fontReady = fontManager.initialize(fontPath);
    return fontReady;
}

void LayoutEngine::shutdown() {
    fontManager.shutdown();
    fontReady = false;
    fontAttempted = true;
}

float LayoutEngine::measureText(
    const std::string& text,
    float fontSize
) {
    if (fontReady) {
        return fontManager.measureText(
            text,
            static_cast<unsigned int>(
                std::max(1.0f, fontSize)
            )
        );
    }

    return fallbackTextWidth(text, fontSize);
}

void LayoutEngine::layoutText(
    LayoutNode& layout,
    const std::string& text,
    float availableWidth
) {
    layout.textFragments.clear();

    std::string word;
    std::string currentLine;

    float currentWidth = 0;
    float maximumWidth = 0;
    float y = 0;

    const float spaceWidth = measureText(" ", layout.fontSize);

    auto finishLine = [&]() {
        if (!currentLine.empty()) {
            layout.textFragments.push_back({
                currentLine,
                0,
                y,
                currentWidth
            });

            maximumWidth = std::max(maximumWidth, currentWidth);
        }

        currentLine.clear();
        currentWidth = 0;
        y += layout.lineHeight;
    };

    auto appendWord = [&](const std::string& value) {
        if (value.empty()) return;

        float wordWidth = measureText(value, layout.fontSize);

        if (!currentLine.empty()) {
            float combinedWidth = currentWidth + spaceWidth + wordWidth;

            if (availableWidth > 0 && combinedWidth > availableWidth) {
                finishLine();
            }
        }

        if (availableWidth > 0 && wordWidth > availableWidth) {
            for (size_t position = 0; position < value.size();) {
                size_t length = std::min(
                    utf8Length(value, position),
                    value.size() - position
                );

                std::string character = value.substr(position, length);
                float characterWidth = measureText(
                    character,
                    layout.fontSize
                );

                if (!currentLine.empty() &&
                    currentWidth + characterWidth > availableWidth) {
                    finishLine();
                }

                currentLine += character;
                currentWidth += characterWidth;
                position += length;
            }

            return;
        }

        if (!currentLine.empty()) {
            currentLine += " ";
            currentWidth += spaceWidth;
        }

        currentLine += value;
        currentWidth += wordWidth;
    };

    for (size_t position = 0; position < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[position]);

        if (c == '\r' || c == '\n') {
            appendWord(word);
            word.clear();

            if (c == '\r' &&
                position + 1 < text.size() &&
                text[position + 1] == '\n') {
                ++position;
            }

            finishLine();
            ++position;
            continue;
        }

        if (std::isspace(c)) {
            appendWord(word);
            word.clear();
            ++position;
            continue;
        }

        size_t length = std::min(
            utf8Length(text, position),
            text.size() - position
        );

        word += text.substr(position, length);
        position += length;
    }

    appendWord(word);

    if (!currentLine.empty() || layout.textFragments.empty()) {
        layout.textFragments.push_back({
            currentLine,
            0,
            y,
            currentWidth
        });

        maximumWidth = std::max(maximumWidth, currentWidth);
        y += layout.lineHeight;
    }

    layout.contentWidth = maximumWidth;
    layout.width = maximumWidth;
    layout.contentHeight = y;
    layout.height = y;
}

std::unique_ptr<LayoutNode> LayoutEngine::build(
    Node* root,
    float viewportWidth
) {
    if (!root) return nullptr;

    if (!fontAttempted) {
        initialize("assets/fonts/Roboto-Regular.ttf");
    }

    return buildNode(root, viewportWidth, 16);
}

std::unique_ptr<LayoutNode> LayoutEngine::buildNode(
    Node* node,
    float availableWidth,
    float inheritedFontSize
) {
    if (!node) return nullptr;

    auto layout = std::make_unique<LayoutNode>();

    layout->domNode = node;
    layout->fontSize = inheritedFontSize;
    layout->lineHeight = inheritedFontSize * 1.2f;

    if (node->type() == NodeType::Text) {
        layout->display = "inline";

        layoutText(
            *layout,
            node->text(),
            availableWidth
        );

        return layout;
    }

    if (node->type() != NodeType::Element) {
        return layout;
    }

    std::string tag = lower(node->name());

    if (isHiddenTag(tag)) {
        layout->display = "none";
        return layout;
    }

    std::string display = node->computedStyle("display");

    if (display.empty()) {
        display = defaultDisplay(tag);
    }

    layout->display = display;

    if (display == "none") {
        return layout;
    }

    std::string fontSizeValue = node->computedStyle("font-size");

    if (!fontSizeValue.empty()) {
        layout->fontSize = parseNumber(
            fontSizeValue,
            inheritedFontSize
        );
    }

    if (layout->fontSize <= 0) {
        layout->fontSize = inheritedFontSize;
    }

    layout->lineHeight = layout->fontSize * 1.2f;

    std::string lineHeightValue = node->computedStyle("line-height");

    if (!lineHeightValue.empty()) {
        float parsed = parseNumber(
            lineHeightValue,
            layout->lineHeight
        );

        if (lineHeightValue.find("px") != std::string::npos) {
            layout->lineHeight = parsed;
        } else if (parsed > 0) {
            layout->lineHeight = parsed * layout->fontSize;
        }
    }

    parseBox(
        node,
        "margin",
        layout->marginTop,
        layout->marginRight,
        layout->marginBottom,
        layout->marginLeft
    );

    parseBox(
        node,
        "padding",
        layout->paddingTop,
        layout->paddingRight,
        layout->paddingBottom,
        layout->paddingLeft
    );

    parseBorders(
        node,
        layout->borderTop,
        layout->borderRight,
        layout->borderBottom,
        layout->borderLeft
    );

    std::string value;

    value = node->computedStyle("margin-top");
    if (!value.empty()) layout->marginTop = parseNumber(value, layout->marginTop);

    value = node->computedStyle("margin-right");
    if (!value.empty()) layout->marginRight = parseNumber(value, layout->marginRight);

    value = node->computedStyle("margin-bottom");
    if (!value.empty()) layout->marginBottom = parseNumber(value, layout->marginBottom);

    value = node->computedStyle("margin-left");
    if (!value.empty()) layout->marginLeft = parseNumber(value, layout->marginLeft);

    value = node->computedStyle("padding-top");
    if (!value.empty()) layout->paddingTop = parseNumber(value, layout->paddingTop);

    value = node->computedStyle("padding-right");
    if (!value.empty()) layout->paddingRight = parseNumber(value, layout->paddingRight);

    value = node->computedStyle("padding-bottom");
    if (!value.empty()) layout->paddingBottom = parseNumber(value, layout->paddingBottom);

    value = node->computedStyle("padding-left");
    if (!value.empty()) layout->paddingLeft = parseNumber(value, layout->paddingLeft);

    float horizontalExtra =
        layout->paddingLeft +
        layout->paddingRight +
        layout->borderLeft +
        layout->borderRight;

    float verticalExtra =
        layout->paddingTop +
        layout->paddingBottom +
        layout->borderTop +
        layout->borderBottom;

    std::string boxSizing = node->computedStyle("box-sizing");
    std::string widthValue = node->computedStyle("width");
    std::string heightValue = node->computedStyle("height");

    bool widthAuto = widthValue.empty() || widthValue == "auto";
    bool inlineDisplay = isInlineDisplay(display);

    float specifiedWidth = parseNumber(widthValue, availableWidth);

    if (widthAuto && !inlineDisplay) {
        specifiedWidth = std::max(
            0.0f,
            availableWidth -
            layout->marginLeft -
            layout->marginRight
        );
    }

    if (boxSizing == "border-box") {
        layout->width = specifiedWidth;
        layout->contentWidth = std::max(
            0.0f,
            specifiedWidth - horizontalExtra
        );
    } else {
        layout->contentWidth = std::max(0.0f, specifiedWidth);
        layout->width = layout->contentWidth + horizontalExtra;
    }

    if (inlineDisplay && widthAuto) {
        layout->contentWidth = 0;
        layout->width = horizontalExtra;
    }

    std::string minWidth = node->computedStyle("min-width");
    std::string maxWidth = node->computedStyle("max-width");

    if (!minWidth.empty()) {
        float minimum = parseNumber(minWidth, 0);

        if (boxSizing == "border-box") {
            layout->width = std::max(layout->width, minimum);
            layout->contentWidth = std::max(
                0.0f,
                layout->width - horizontalExtra
            );
        } else {
            layout->contentWidth = std::max(
                layout->contentWidth,
                minimum
            );

            layout->width = layout->contentWidth + horizontalExtra;
        }
    }

    if (!maxWidth.empty()) {
        float maximum = parseNumber(maxWidth, layout->width);

        layout->width = std::min(layout->width, maximum);

        if (boxSizing != "border-box") {
            layout->contentWidth = std::max(
                0.0f,
                layout->width - horizontalExtra
            );
        } else {
            layout->contentWidth = std::max(
                0.0f,
                layout->width - horizontalExtra
            );
        }
    }

    float contentAvailableWidth = std::max(
        0.0f,
        layout->contentWidth
    );

    float currentY = 0;
    float lineX = 0;
    float currentLineHeight = 0;
    float inlineContentWidth = 0;

    float contentStartX =
        layout->borderLeft +
        layout->paddingLeft;

    float contentStartY =
        layout->borderTop +
        layout->paddingTop;

    for (const auto& child : node->children()) {
        auto childLayout = buildNode(
            child.get(),
            contentAvailableWidth > 0
                ? contentAvailableWidth
                : availableWidth,
            layout->fontSize
        );

        if (!childLayout || childLayout->display == "none") {
            continue;
        }

        bool inlineChild = isInlineDisplay(childLayout->display);

        float outerWidth =
            childLayout->width +
            childLayout->marginLeft +
            childLayout->marginRight;

        float outerHeight =
            childLayout->height +
            childLayout->marginTop +
            childLayout->marginBottom;

        if (inlineChild) {
            if (lineX > 0 &&
                lineX + outerWidth > contentAvailableWidth &&
                contentAvailableWidth > 0) {
                currentY += currentLineHeight;
                lineX = 0;
                currentLineHeight = 0;
            }

            childLayout->x =
                contentStartX +
                lineX +
                childLayout->marginLeft;

            childLayout->y =
                contentStartY +
                currentY +
                childLayout->marginTop;

            lineX += outerWidth;

            currentLineHeight = std::max(
                currentLineHeight,
                outerHeight
            );

            inlineContentWidth = std::max(
                inlineContentWidth,
                lineX
            );
        } else {
            if (lineX > 0) {
                currentY += currentLineHeight;
                lineX = 0;
                currentLineHeight = 0;
            }

            currentY += childLayout->marginTop;

            childLayout->x =
                contentStartX +
                childLayout->marginLeft;

            childLayout->y =
                contentStartY +
                currentY;

            currentY +=
                childLayout->height +
                childLayout->marginBottom;
        }

        layout->children.push_back(std::move(childLayout));
    }

    if (lineX > 0) {
        currentY += currentLineHeight;
    }

    if (inlineDisplay && widthAuto) {
        layout->contentWidth = std::min(
            inlineContentWidth,
            availableWidth
        );

        layout->width = layout->contentWidth + horizontalExtra;
    }

    float naturalHeight = std::max(
        currentY,
        calculateHeight(node)
    );

    if (!heightValue.empty() && heightValue != "auto") {
        float specifiedHeight = parseNumber(
            heightValue,
            naturalHeight
        );

        if (boxSizing == "border-box") {
            layout->height = std::max(0.0f, specifiedHeight);
            layout->contentHeight = std::max(
                0.0f,
                layout->height - verticalExtra
            );
        } else {
            layout->contentHeight = std::max(0.0f, specifiedHeight);
            layout->height = layout->contentHeight + verticalExtra;
        }
    } else {
        layout->contentHeight = naturalHeight;
        layout->height = layout->contentHeight + verticalExtra;
    }

    std::string minHeight = node->computedStyle("min-height");
    std::string maxHeight = node->computedStyle("max-height");

    if (!minHeight.empty()) {
        float minimum = parseNumber(minHeight, 0);

        if (boxSizing == "border-box") {
            layout->height = std::max(layout->height, minimum);
            layout->contentHeight = std::max(
                0.0f,
                layout->height - verticalExtra
            );
        } else {
            layout->contentHeight = std::max(
                layout->contentHeight,
                minimum
            );

            layout->height = layout->contentHeight + verticalExtra;
        }
    }

    if (!maxHeight.empty()) {
        float maximum = parseNumber(maxHeight, layout->height);

        layout->height = std::min(layout->height, maximum);
        layout->contentHeight = std::max(
            0.0f,
            layout->height - verticalExtra
        );
    }

    return layout;
}

float LayoutEngine::calculateHeight(Node* node) {
    if (!node || node->type() != NodeType::Element) {
        return 0;
    }

    std::string tag = lower(node->name());

    if (tag == "h1") return 38;
    if (tag == "h2") return 32;
    if (tag == "h3") return 28;
    if (tag == "h4") return 26;
    if (tag == "h5") return 24;
    if (tag == "h6") return 22;
    if (tag == "p") return 24;
    if (tag == "li") return 24;
    if (tag == "img") return 150;

    return 0;
}

float LayoutEngine::parseDimension(
    const std::string& value,
    float fallback
) {
    return parseNumber(value, fallback);
}

}
