
#pragma once

#include <string>
#include <cstddef>

namespace Veltrix {

enum class CSSTokenType {
    Ident,
    Function,
    AtKeyword,
    Hash,
    String,
    BadString,
    URL,
    BadURL,
    Delim,
    Number,
    Percentage,
    Dimension,
    Whitespace,
    CDO,
    CDC,
    Colon,
    Semicolon,
    Comma,
    OpenBracket,
    CloseBracket,
    OpenParen,
    CloseParen,
    OpenBrace,
    CloseBrace,
    EndOfFile
};

struct CSSToken {
    CSSTokenType type = CSSTokenType::EndOfFile;
    std::string value;
    std::string unit;
    double number = 0.0;
    bool integer = false;
    bool idHash = false;
    char delimiter = '\0';
    size_t position = 0;
};

}
