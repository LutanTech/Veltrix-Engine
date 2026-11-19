#include "parser.h"
#include <algorithm>
#include <cctype>

namespace Veltrix {

std::vector<CSSRule> CSSParser::parse(const std::string& source) {
    tokens = tokenizer.tokenize(source);
    position = 0;

    return parseRuleList();
}

const CSSToken& CSSParser::current() const {
    static const CSSToken eof;
    return position < tokens.size() ? tokens[position] : eof;
}

bool CSSParser::isEOF() const {
    return current().type == CSSTokenType::EndOfFile;
}

void CSSParser::advance() {
    if (!isEOF()) {
        ++position;
    }
}

void CSSParser::skipWhitespace() {
    while (!isEOF() &&
           current().type == CSSTokenType::Whitespace) {
        advance();
    }
}

std::vector<CSSToken> CSSParser::consumePrelude() {
    std::vector<CSSToken> result;

    int parentheses = 0;
    int brackets = 0;

    while (!isEOF()) {
        const CSSToken& token = current();

        if (token.type == CSSTokenType::OpenParen) {
            ++parentheses;
        }
        else if (token.type == CSSTokenType::CloseParen) {
            parentheses = std::max(0, parentheses - 1);
        }
        else if (token.type == CSSTokenType::OpenBracket) {
            ++brackets;
        }
        else if (token.type == CSSTokenType::CloseBracket) {
            brackets = std::max(0, brackets - 1);
        }

        if (parentheses == 0 && brackets == 0) {
            if (token.type == CSSTokenType::OpenBrace ||
                token.type == CSSTokenType::Semicolon ||
                token.type == CSSTokenType::CloseBrace) {
                break;
            }
        }

        result.push_back(token);
        advance();
    }

    return result;
}

std::vector<CSSToken> CSSParser::consumeBlock() {
    std::vector<CSSToken> result;

    if (current().type != CSSTokenType::OpenBrace) {
        return result;
    }

    advance();

    int braces = 1;
    int parentheses = 0;
    int brackets = 0;

    while (!isEOF() && braces > 0) {
        CSSToken token = current();
        advance();

        if (token.type == CSSTokenType::OpenParen) {
            ++parentheses;
        }
        else if (token.type == CSSTokenType::CloseParen) {
            parentheses = std::max(0, parentheses - 1);
        }
        else if (token.type == CSSTokenType::OpenBracket) {
            ++brackets;
        }
        else if (token.type == CSSTokenType::CloseBracket) {
            brackets = std::max(0, brackets - 1);
        }
        else if (parentheses == 0 && brackets == 0) {
            if (token.type == CSSTokenType::OpenBrace) {
                ++braces;
            }
            else if (token.type == CSSTokenType::CloseBrace) {
                --braces;

                if (braces == 0) {
                    break;
                }
            }
        }

        result.push_back(std::move(token));
    }

    return result;
}

std::vector<CSSRule> CSSParser::parseRuleList() {
    std::vector<CSSRule> rules;

    while (!isEOF()) {
        skipWhitespace();

        if (isEOF()) {
            break;
        }

        std::vector<CSSToken> prelude = consumePrelude();

        if (current().type == CSSTokenType::Semicolon) {
            advance();
            continue;
        }

        if (current().type != CSSTokenType::OpenBrace) {
            if (!isEOF()) {
                advance();
            }

            continue;
        }

        std::string selector = trim(
            serialize(prelude, 0, prelude.size())
        );

        std::vector<CSSToken> block = consumeBlock();

        if (selector.empty() || selector[0] == '@') {
            continue;
        }

        Style style = parseDeclarations(block);

        std::vector<CSSToken> selectorTokens;
        int parentheses = 0;
        int brackets = 0;

        for (const auto& token : prelude) {
            if (token.type == CSSTokenType::OpenParen) {
                ++parentheses;
            }
            else if (token.type == CSSTokenType::CloseParen) {
                parentheses = std::max(0, parentheses - 1);
            }
            else if (token.type == CSSTokenType::OpenBracket) {
                ++brackets;
            }
            else if (token.type == CSSTokenType::CloseBracket) {
                brackets = std::max(0, brackets - 1);
            }

            if (token.type == CSSTokenType::Comma &&
                parentheses == 0 &&
                brackets == 0) {
                std::string part = trim(
                    serialize(selectorTokens, 0, selectorTokens.size())
                );

                if (!part.empty()) {
                    rules.push_back({part, style});
                }

                selectorTokens.clear();
                continue;
            }

            selectorTokens.push_back(token);
        }

        std::string lastSelector = trim(
            serialize(selectorTokens, 0, selectorTokens.size())
        );

        if (!lastSelector.empty()) {
            rules.push_back({lastSelector, style});
        }
    }

    return rules;
}

Style CSSParser::parseDeclarations(
    const std::vector<CSSToken>& declarations
) {
    Style style;

    std::vector<CSSToken> declaration;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;

    for (const auto& token : declarations) {
        if (token.type == CSSTokenType::OpenParen) {
            ++parentheses;
        }
        else if (token.type == CSSTokenType::CloseParen) {
            parentheses = std::max(0, parentheses - 1);
        }
        else if (token.type == CSSTokenType::OpenBracket) {
            ++brackets;
        }
        else if (token.type == CSSTokenType::CloseBracket) {
            brackets = std::max(0, brackets - 1);
        }
        else if (token.type == CSSTokenType::OpenBrace) {
            ++braces;
        }
        else if (token.type == CSSTokenType::CloseBrace) {
            braces = std::max(0, braces - 1);
        }

        if (token.type == CSSTokenType::Semicolon &&
            parentheses == 0 &&
            brackets == 0 &&
            braces == 0) {
            parseDeclaration(declaration, style);
            declaration.clear();
            continue;
        }

        declaration.push_back(token);
    }

    parseDeclaration(declaration, style);

    return style;
}

void CSSParser::parseDeclaration(
    const std::vector<CSSToken>& declaration,
    Style& style
) {
    if (declaration.empty()) {
        return;
    }

    int parentheses = 0;
    int brackets = 0;
    int braces = 0;

    for (size_t i = 0; i < declaration.size(); ++i) {
        const auto& token = declaration[i];

        if (token.type == CSSTokenType::OpenParen) {
            ++parentheses;
        }
        else if (token.type == CSSTokenType::CloseParen) {
            parentheses = std::max(0, parentheses - 1);
        }
        else if (token.type == CSSTokenType::OpenBracket) {
            ++brackets;
        }
        else if (token.type == CSSTokenType::CloseBracket) {
            brackets = std::max(0, brackets - 1);
        }
        else if (token.type == CSSTokenType::OpenBrace) {
            ++braces;
        }
        else if (token.type == CSSTokenType::CloseBrace) {
            braces = std::max(0, braces - 1);
        }

        if (token.type != CSSTokenType::Colon ||
            parentheses != 0 ||
            brackets != 0 ||
            braces != 0) {
            continue;
        }

        std::string name = trim(
            serialize(declaration, 0, i)
        );

        std::string value = trim(
            serialize(declaration, i + 1, declaration.size())
        );

        if (name.empty() || value.empty()) {
            return;
        }

        std::transform(
            name.begin(),
            name.end(),
            name.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        style.setProperty(name, value);
        return;
    }
}

std::string CSSParser::serialize(
    const std::vector<CSSToken>& source,
    size_t start,
    size_t end
) const {
    std::string result;

    end = std::min(end, source.size());

    for (size_t i = start; i < end; ++i) {
        result += serializeToken(source[i]);
    }

    return result;
}

std::string CSSParser::serializeToken(const CSSToken& token) const {
    switch (token.type) {
        case CSSTokenType::Ident:
            return token.value;

        case CSSTokenType::Function:
            return token.value + "(";

        case CSSTokenType::AtKeyword:
            return "@" + token.value;

        case CSSTokenType::Hash:
            return "#" + token.value;

        case CSSTokenType::String:
            return "\"" + token.value + "\"";

        case CSSTokenType::BadString:
            return "";

        case CSSTokenType::Number:
        case CSSTokenType::Percentage:
        case CSSTokenType::Dimension:
            return token.value;

        case CSSTokenType::Whitespace:
            return " ";

        case CSSTokenType::Colon:
            return ":";

        case CSSTokenType::Semicolon:
            return ";";

        case CSSTokenType::Comma:
            return ",";

        case CSSTokenType::OpenBrace:
            return "{";

        case CSSTokenType::CloseBrace:
            return "}";

        case CSSTokenType::OpenBracket:
            return "[";

        case CSSTokenType::CloseBracket:
            return "]";

        case CSSTokenType::OpenParen:
            return "(";

        case CSSTokenType::CloseParen:
            return ")";

        case CSSTokenType::Delim:
            return std::string(1, token.delimiter);

        default:
            return "";
    }
}

std::string CSSParser::trim(const std::string& value) const {
    size_t start = value.find_first_not_of(" \t\n\r\f");

    if (start == std::string::npos) {
        return "";
    }

    size_t end = value.find_last_not_of(" \t\n\r\f");

    return value.substr(start, end - start + 1);
}

}