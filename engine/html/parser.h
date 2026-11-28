
#pragma once

#include "../dom/document.h"
#include <string>

namespace Veltrix {

class HTMLParser {
public:
    Document parse(const std::string& html);
};

}
