#pragma once

#include "tokenizer.h"
#include "style.h"
#include <string>
#include <vector>

namespace Veltrix {

struct CSSRule {
    std::string selector;
    Style style;
};

class CSSParser {
public:
    std::vector<CSSRule> parse(const std::string& source);

private:
    CSSTokenizer tokenizer;
    std::vector<CSSToken> tokens;
    size_t position = 0;

    const CSSToken& current() const;
    bool isEOF() const;
    void advance();
    void skipWhitespace();

    std::vector<CSSToken> consumePrelude();
    std::vector<CSSToken> consumeBlock();

    std::vector<CSSRule> parseRuleList();
    Style parseDeclarations(const std::vector<CSSToken>& declarations);

    void parseDeclaration(
        const std::vector<CSSToken>& declaration,
        Style& style
    );

    std::string serialize(
        const std::vector<CSSToken>& source,
        size_t start,
        size_t end
    ) const;

    std::string serializeToken(const CSSToken& token) const;
    std::string trim(const std::string& value) const;
};

}