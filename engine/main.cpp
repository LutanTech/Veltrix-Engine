
#include <iostream>
#include <functional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/engine.h"
#include "html/parser.h"
#include "css/parser.h"
#include "css/style_engine.h"
#include "layout/layout_engine.h"
#include "render/display_list.h"
#include "render/display_list_builder.h"

void printNode(const Veltrix::Node* node, int depth = 0) {
    if (!node) return;

    std::string indentation(depth * 2, ' ');

    if (node->type() == Veltrix::NodeType::Element) {
        std::cout << indentation << "<" << node->name();

        for (const std::string& attribute : {"id", "class", "lang"}) {
            if (node->hasAttribute(attribute)) {
                std::cout << " " << attribute << "=\"" << node->attribute(attribute) << "\"";
            }
        }

        std::cout << ">\n";
    }

    if (node->type() == Veltrix::NodeType::Text) {
        std::cout << indentation << node->text() << "\n";
    }

    for (const auto& child : node->children()) {
        printNode(child.get(), depth + 1);
    }
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);

    if (!file) {
        throw std::runtime_error("Could not open: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

void extractStyleSheets(
    const Veltrix::Node* node,
    std::string& css
) {
    if (!node) return;

    if (
        node->type() == Veltrix::NodeType::Element &&
        node->name() == "style"
    ) {
        for (const auto& child : node->children()) {
            if (child->type() == Veltrix::NodeType::Text) {
                css += child->text();
                css += "\n";
            }
        }

        return;
    }

    for (const auto& child : node->children()) {
        extractStyleSheets(child.get(), css);
    }
}

int main() {
    Veltrix::Engine engine;

    if (!engine.initialize()) {
        return 1;
    }

    try {
        Veltrix::HTMLParser parser;
        Veltrix::CSSParser cssParser;

        std::string html = readFile("index.html");
        auto document = parser.parse(html);

        std::cout << "[VelHTML] Document parsed.\n";

        std::string css;

        extractStyleSheets(document.root(), css);

        auto rules = cssParser.parse(css);

        std::cout << "[VelCSS] Extracted "
                  << rules.size()
                  << " CSS rule(s) from HTML.\n";

        Veltrix::StyleEngine styleEngine;
        styleEngine.compute(document, rules);

        std::cout << "[VelCSS] Computed styles.\n";

        Veltrix::LayoutEngine layoutEngine;
        auto layout = layoutEngine.build(document.root(), 1280);

        if (!layout) {
            throw std::runtime_error("Could not build layout tree.");
        }

        std::cout << "[VelLayout] Layout tree:\n";

        std::function<void(const Veltrix::LayoutNode*, int)> printLayout =
            [&](const Veltrix::LayoutNode* node, int depth) {
                if (!node) return;

                std::string indentation(depth * 2, ' ');

                if (node->domNode->type() == Veltrix::NodeType::Element) {
                    std::cout << indentation
                              << node->domNode->name()
                              << " x=" << node->x
                              << " y=" << node->y
                              << " width=" << node->width
                              << " height=" << node->height
                              << " font-size=" << node->fontSize
                              << "\n";
                }

                for (const auto& child : node->children) {
                    printLayout(child.get(), depth + 1);
                }
            };

        printLayout(layout.get(), 0);

        std::cout << "[VelHTML] Parsed document:\n";
        printNode(document.root());

        std::cout << "[VelRender] Building display list...\n";

        Veltrix::DisplayListBuilder displayListBuilder;
        Veltrix::DisplayList displayList = displayListBuilder.build(layout.get());

        std::cout << "[VelRender] Generated "
                  << displayList.rectangles().size()
                  << " rectangle(s) and "
                  << displayList.texts().size()
                  << " text command(s).\n";

        std::cout << "[VelCore] Entering runtime...\n";

        engine.setDisplayList(displayList);
        engine.run();
    }
    catch (const std::exception& error) {
        std::cerr << "[VelCore] " << error.what() << "\n";
        engine.shutdown();
        return 1;
    }

    engine.shutdown();

    return 0;
}
