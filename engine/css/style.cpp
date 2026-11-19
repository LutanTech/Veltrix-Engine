#include "style.h"

namespace Veltrix {

void Style::setProperty(const std::string& name, const std::string& value) {
    properties[name] = value;
}

bool Style::hasProperty(const std::string& name) const {
    return properties.contains(name);
}

std::string Style::getProperty(const std::string& name) const {
    auto it = properties.find(name);
    return it == properties.end() ? "" : it->second;
}

}