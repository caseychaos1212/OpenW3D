#pragma once

#include <string>
#include <string_view>

namespace leveledit_qt {

class SettingsBridge
{
public:
    int readInt(std::string_view section, std::string_view key, int fallback) const;
    std::string readString(std::string_view section, std::string_view key, std::string_view fallback) const;

    void writeInt(std::string_view section, std::string_view key, int value);
    void writeString(std::string_view section, std::string_view key, std::string_view value);
};

} // namespace leveledit_qt
