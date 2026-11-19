#include "tokenizer.h"
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <limits>

namespace Veltrix {

bool CSSTokenizer::isEOF() const {
    return position >= input.size();
}

char CSSTokenizer::current() const {
    return isEOF() ? '\0' : input[position];
}

char CSSTokenizer::peek(size_t offset) const {
    size_t index = position + offset;
    return index >= input.size() ? '\0' : input[index];
}

char CSSTokenizer::consume() {
    return isEOF() ? '\0' : input[position++];
}

bool CSSTokenizer::isWhitespace(char c) const {
    return c == ' ' || c == '\n' || c == '\t' ||
           c == '\r' || c == '\f';
}

bool CSSTokenizer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool CSSTokenizer::isHexDigit(char c) const {
    return isDigit(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

bool CSSTokenizer::isNameStart(char c) const {
    unsigned char value = static_cast<unsigned char>(c);

    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_' || value >= 0x80;
}

bool CSSTokenizer::isName(char c) const {
    return isNameStart(c) || isDigit(c) || c == '-';
}

CSSToken CSSTokenizer::makeToken(
    CSSTokenType type,
    std::string value
) const {
    CSSToken token;
    token.type = type;
    token.value = std::move(value);
    token.position = position;
    return token;
}

bool CSSTokenizer::startsIdentifier() const {
    char a = current();
    char b = peek();

    if (a == '-') {
        return isNameStart(b) || b == '-' ||
               (b == '\\' && peek(2) != '\n' && peek(2) != '\0');
    }

    if (isNameStart(a)) {
        return true;
    }

    return a == '\\' && b != '\n' && b != '\0';
}

bool CSSTokenizer::startsNumber() const {
    char a = current();
    char b = peek();
    char c = peek(2);

    if (a == '+' || a == '-') {
        return isDigit(b) || (b == '.' && isDigit(c));
    }

    if (a == '.') {
        return isDigit(b);
    }

    return isDigit(a);
}

void CSSTokenizer::consumeWhitespace() {
    while (!isEOF() && isWhitespace(current())) {
        consume();
    }
}

void CSSTokenizer::consumeComment() {
    if (current() != '/' || peek() != '*') {
        return;
    }

    consume();
    consume();

    while (!isEOF()) {
        if (current() == '*' && peek() == '/') {
            consume();
            consume();
            return;
        }

        consume();
    }
}

void CSSTokenizer::appendCodePoint(
    std::string& output,
    unsigned int codePoint
) {
    if (codePoint == 0 || codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        codePoint = 0xFFFD;
    }

    if (codePoint <= 0x7F) {
        output += static_cast<char>(codePoint);
    }
    else if (codePoint <= 0x7FF) {
        output += static_cast<char>(0xC0 | (codePoint >> 6));
        output += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0xFFFF) {
        output += static_cast<char>(0xE0 | (codePoint >> 12));
        output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else {
        output += static_cast<char>(0xF0 | (codePoint >> 18));
        output += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
}

std::string CSSTokenizer::consumeEscape() {
    if (current() != '\\') {
        return "";
    }

    consume();

    if (isEOF()) {
        return "\xEF\xBF\xBD";
    }

    if (current() == '\n' || current() == '\r' || current() == '\f') {
        return "";
    }

    if (isHexDigit(current())) {
        unsigned int codePoint = 0;
        int count = 0;

        while (!isEOF() && isHexDigit(current()) && count < 6) {
            char c = consume();

            unsigned int digit = 0;

            if (c >= '0' && c <= '9') {
                digit = c - '0';
            }
            else if (c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            }
            else {
                digit = c - 'A' + 10;
            }

            codePoint = codePoint * 16 + digit;
            ++count;
        }

        if (!isEOF() && isWhitespace(current())) {
            if (current() == '\r' && peek() == '\n') {
                consume();
            }

            consume();
        }

        std::string output;
        appendCodePoint(output, codePoint);
        return output;
    }

    std::string output;
    output += consume();
    return output;
}

std::string CSSTokenizer::consumeName() {
    std::string result;

    while (!isEOF()) {
        if (isName(current())) {
            result += consume();
        }
        else if (current() == '\\' &&
                 peek() != '\n' &&
                 peek() != '\r' &&
                 peek() != '\f' &&
                 peek() != '\0') {
            result += consumeEscape();
        }
        else {
            break;
        }
    }

    return result;
}

std::string CSSTokenizer::consumeString(char quote) {
    std::string result;

    consume();

    while (!isEOF()) {
        char c = current();

        if (c == quote) {
            consume();
            break;
        }

        if (c == '\n' || c == '\r' || c == '\f') {
            break;
        }

        if (c == '\\') {
            consume();

            if (isEOF()) {
                break;
            }

            if (current() == '\n') {
                consume();
                continue;
            }

            if (current() == '\r') {
                consume();

                if (current() == '\n') {
                    consume();
                }

                continue;
            }

            if (current() == '\f') {
                consume();
                continue;
            }

            --position;
            result += consumeEscape();
            continue;
        }

        result += consume();
    }

    return result;
}

void CSSTokenizer::consumeNumber(
    double& value,
    bool& integer
) {
    size_t start = position;

    if (current() == '+' || current() == '-') {
        consume();
    }

    while (isDigit(current()) && !isEOF()) {
        consume();
    }

    integer = true;

    if (current() == '.' && isDigit(peek())) {
        integer = false;
        consume();

        while (isDigit(current()) && !isEOF()) {
            consume();
        }
    }

    if ((current() == 'e' || current() == 'E') &&
        (isDigit(peek()) ||
         ((peek() == '+' || peek() == '-') && isDigit(peek(2))))) {
        integer = false;
        consume();

        if (current() == '+' || current() == '-') {
            consume();
        }

        while (isDigit(current()) && !isEOF()) {
            consume();
        }
    }

    std::string numberText = input.substr(start, position - start);

    char* end = nullptr;
    value = std::strtod(numberText.c_str(), &end);

    if (!end || end == numberText.c_str() ||
        !std::isfinite(value)) {
        value = 0;
    }
}

CSSToken CSSTokenizer::consumeNumericToken() {
    size_t start = position;

    double value = 0;
    bool integer = false;

    consumeNumber(value, integer);

    if (startsIdentifier()) {
        std::string unit = consumeName();

        CSSToken token = makeToken(
            CSSTokenType::Dimension,
            input.substr(start, position - start)
        );

        token.number = value;
        token.integer = integer;
        token.unit = std::move(unit);

        return token;
    }

    if (current() == '%') {
        consume();

        CSSToken token = makeToken(
            CSSTokenType::Percentage,
            input.substr(start, position - start)
        );

        token.number = value;
        token.integer = integer;

        return token;
    }

    CSSToken token = makeToken(
        CSSTokenType::Number,
        input.substr(start, position - start)
    );

    token.number = value;
    token.integer = integer;

    return token;
}

CSSToken CSSTokenizer::consumeIdentLikeToken() {
    std::string name = consumeName();

    if (current() == '(') {
        consume();

        if (name == "url" || name == "URL") {
            return makeToken(
                CSSTokenType::Function,
                name
            );
        }

        return makeToken(
            CSSTokenType::Function,
            name
        );
    }

    return makeToken(
        CSSTokenType::Ident,
        name
    );
}

CSSToken CSSTokenizer::consumeHashToken() {
    consume();

    CSSToken token = makeToken(
        CSSTokenType::Hash,
        consumeName()
    );

    token.idHash = startsIdentifier();

    return token;
}

CSSToken CSSTokenizer::consumeAtKeywordToken() {
    consume();

    return makeToken(
        CSSTokenType::AtKeyword,
        consumeName()
    );
}

CSSToken CSSTokenizer::consumeStringToken(char quote) {
    size_t start = position;
    std::string value = consumeString(quote);

    bool terminated = position > start + 1 &&
                      input[position - 1] == quote;

    return makeToken(
        terminated ? CSSTokenType::String : CSSTokenType::BadString,
        value
    );
}

std::vector<CSSToken> CSSTokenizer::tokenize(
    const std::string& source
) {
    input = source;
    position = 0;

    std::vector<CSSToken> tokens;

    while (!isEOF()) {
        char c = current();

        if (isWhitespace(c)) {
            consumeWhitespace();
            tokens.push_back(makeToken(CSSTokenType::Whitespace));
            continue;
        }

        if (c == '/' && peek() == '*') {
            consumeComment();
            continue;
        }

        if (c == '"' || c == '\'') {
            tokens.push_back(consumeStringToken(c));
            continue;
        }

        if (c == '#') {
            if (isName(peek()) || peek() == '\\') {
                tokens.push_back(consumeHashToken());
            }
            else {
                consume();
                CSSToken token = makeToken(CSSTokenType::Delim, "#");
                token.delimiter = '#';
                tokens.push_back(std::move(token));
            }

            continue;
        }

        if (c == '@') {
            if (startsIdentifier()) {
                tokens.push_back(consumeAtKeywordToken());
            }
            else {
                consume();
                CSSToken token = makeToken(CSSTokenType::Delim, "@");
                token.delimiter = '@';
                tokens.push_back(std::move(token));
            }

            continue;
        }

        if (startsNumber()) {
            tokens.push_back(consumeNumericToken());
            continue;
        }

        if (startsIdentifier()) {
            tokens.push_back(consumeIdentLikeToken());
            continue;
        }

        CSSToken token;

        switch (c) {
            case ':':
                consume();
                token = makeToken(CSSTokenType::Colon, ":");
                break;

            case ';':
                consume();
                token = makeToken(CSSTokenType::Semicolon, ";");
                break;

            case ',':
                consume();
                token = makeToken(CSSTokenType::Comma, ",");
                break;

            case '{':
                consume();
                token = makeToken(CSSTokenType::OpenBrace, "{");
                break;

            case '}':
                consume();
                token = makeToken(CSSTokenType::CloseBrace, "}");
                break;

            case '[':
                consume();
                token = makeToken(CSSTokenType::OpenBracket, "[");
                break;

            case ']':
                consume();
                token = makeToken(CSSTokenType::CloseBracket, "]");
                break;

            case '(':
                consume();
                token = makeToken(CSSTokenType::OpenParen, "(");
                break;

            case ')':
                consume();
                token = makeToken(CSSTokenType::CloseParen, ")");
                break;

            default:
                consume();
                token = makeToken(
                    CSSTokenType::Delim,
                    std::string(1, c)
                );
                token.delimiter = c;
                break;
        }

        tokens.push_back(std::move(token));
    }

    tokens.push_back(makeToken(CSSTokenType::EndOfFile));

    return tokens;
}

}