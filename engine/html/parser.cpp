
#include "parser.h"
#include <lexbor/html/html.h>
#include <lexbor/html/serialize.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/dom/interfaces/attr.h>
#include <iostream>
#include <memory>
#include <string>

namespace Veltrix {

namespace {

std::unique_ptr<Node> convertNode(lxb_dom_node_t* source) {
    if (!source) {
        return nullptr;
    }

    lxb_dom_node_type_t type = lxb_dom_node_type(source);

    if (type == LXB_DOM_NODE_TYPE_TEXT) {
        size_t length = 0;
        const lxb_char_t* content = lxb_dom_node_text_content(source, &length);

        auto node = std::make_unique<Node>(NodeType::Text);

        if (content && length > 0) {
            node->setText(std::string(reinterpret_cast<const char*>(content), length));
        }

        return node;
    }

    if (type != LXB_DOM_NODE_TYPE_ELEMENT) {
        return nullptr;
    }

    auto element = lxb_dom_interface_element(source);

    size_t nameLength = 0;
    const lxb_char_t* name = lxb_dom_element_local_name(element, &nameLength);

    if (!name || nameLength == 0) {
        return nullptr;
    }

    auto node = std::make_unique<Node>(NodeType::Element);
    node->setName(std::string(reinterpret_cast<const char*>(name), nameLength));

    for (
        lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(element);
        attr;
        attr = lxb_dom_element_next_attribute(attr)
    ) {
        size_t attrNameLength = 0;
        size_t attrValueLength = 0;

        const lxb_char_t* attrName = lxb_dom_attr_qualified_name(attr, &attrNameLength);
        const lxb_char_t* attrValue = lxb_dom_attr_value(attr, &attrValueLength);

        if (!attrName) {
            continue;
        }

        std::string attributeName(reinterpret_cast<const char*>(attrName), attrNameLength);
        std::string attributeValue;

        if (attrValue && attrValueLength > 0) {
            attributeValue.assign(reinterpret_cast<const char*>(attrValue), attrValueLength);
        }

        node->setAttribute(attributeName, attributeValue);
    }

    for (
        lxb_dom_node_t* child = lxb_dom_node_first_child(source);
        child;
        child = lxb_dom_node_next(child)
    ) {
        auto convertedChild = convertNode(child);

        if (convertedChild) {
            node->appendChild(std::move(convertedChild));
        }
    }

    return node;
}

}

Document HTMLParser::parse(const std::string& html) {
    Document result;

    lxb_html_document_t* lexborDocument = lxb_html_document_create();

    if (!lexborDocument) {
        std::cerr << "[HTML] Failed to create Lexbor document\n";
        return result;
    }

    lxb_status_t status = lxb_html_document_parse(
        lexborDocument,
        reinterpret_cast<const lxb_char_t*>(html.data()),
        html.size()
    );

    if (status != LXB_STATUS_OK) {
        std::cerr << "[HTML] Lexbor parsing failed: " << status << '\n';
        lxb_html_document_destroy(lexborDocument);
        return result;
    }

    lxb_dom_node_t* documentNode = lxb_dom_interface_node(lexborDocument);

    lexbor_str_t serialized = {0};

    if (lxb_html_serialize_pretty_deep_str(
        documentNode,
        LXB_HTML_SERIALIZE_OPT_WITHOUT_CLOSING,
        2,
        &serialized
    ) == LXB_STATUS_OK && serialized.data) {
        lexbor_str_destroy(&serialized, lexborDocument->dom_document.text, false);
    }

    for (
        lxb_dom_node_t* child = lxb_dom_node_first_child(documentNode);
        child;
        child = lxb_dom_node_next(child)
    ) {
        if (lxb_dom_node_type(child) == LXB_DOM_NODE_TYPE_ELEMENT) {
            result.setRoot(convertNode(child));
            break;
        }
    }

    lxb_html_document_destroy(lexborDocument);

    return result;
}

}
