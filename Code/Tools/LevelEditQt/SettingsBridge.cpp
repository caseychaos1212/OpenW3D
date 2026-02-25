#include "SettingsBridge.h"
#include "ConfigSettings.h"

#include <QString>

namespace leveledit_qt {

namespace {

QString BuildKey(std::string_view section, std::string_view key)
{
    return QString::fromUtf8(section.data(), static_cast<int>(section.size())) +
        QLatin1Char('/') +
        QString::fromUtf8(key.data(), static_cast<int>(key.size()));
}

} // namespace

int SettingsBridge::readInt(std::string_view section, std::string_view key, int fallback) const
{
    QSettings settings = OpenLevelEditSettings();
    return settings.value(BuildKey(section, key), fallback).toInt();
}

std::string SettingsBridge::readString(std::string_view section,
                                       std::string_view key,
                                       std::string_view fallback) const
{
    QSettings settings = OpenLevelEditSettings();
    const QString value = settings.value(
        BuildKey(section, key),
        QString::fromUtf8(fallback.data(), static_cast<int>(fallback.size()))).toString();
    return value.toStdString();
}

void SettingsBridge::writeInt(std::string_view section, std::string_view key, int value)
{
    QSettings settings = OpenLevelEditSettings();
    settings.setValue(BuildKey(section, key), value);
}

void SettingsBridge::writeString(std::string_view section, std::string_view key, std::string_view value)
{
    QSettings settings = OpenLevelEditSettings();
    settings.setValue(BuildKey(section, key),
                      QString::fromUtf8(value.data(), static_cast<int>(value.size())));
}

} // namespace leveledit_qt
