
#pragma once

#include <string>
#include <unordered_map>

namespace Veltrix {

class Style {
public:
    void setProperty(const std::string& name, const std::string& value);
    bool hasProperty(const std::string& name) const;
    std::string getProperty(const std::string& name) const;

    const std::unordered_map<std::string, std::string>& allProperties() const {
        return properties;
    }

private:
    std::unordered_map<std::string, std::string> properties;
};

}
