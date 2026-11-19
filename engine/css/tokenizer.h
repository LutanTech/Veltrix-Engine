#pragma once

#include "token.h"
#include <string>
#include <vector>
#include <cstddef>

namespace Veltrix {

class CSSTokenizer {
public:
    std::vector<CSSToken> tokenize(const std::string& source);

private:
    std::string input;
    size_t position = 0;

    bool isEOF() const;
    char current() const;
    char peek(size_t offset = 1) const;
    char consume();

    bool isWhitespace(char c) const;
    bool isDigit(char c) const;
    bool isHexDigit(char c) const;
    bool isNameStart(char c) const;
    bool isName(char c) const;

    bool startsIdentifier() const;
    bool startsNumber() const;

    void consumeWhitespace();
    void consumeComment();

    std::string consumeName();
    std::string consumeEscape();
    std::string consumeString(char quote);

    CSSToken consumeNumericToken();
    CSSToken consumeIdentLikeToken();
    CSSToken consumeHashToken();
    CSSToken consumeAtKeywordToken();
    CSSToken consumeStringToken(char quote);

    void consumeNumber(
        double& value,
        bool& integer
    );

    void appendCodePoint(
        std::string& output,
        unsigned int codePoint
    );

    CSSToken makeToken(
        CSSTokenType type,
        std::string value = ""
    ) const;
};

}