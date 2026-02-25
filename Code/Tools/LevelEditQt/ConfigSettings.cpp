#include "ConfigSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>

namespace leveledit_qt {

QString LevelEditSettingsPath()
{
    const QString env_openw3d =
        QString::fromLocal8Bit(qgetenv("OPENW3D_LEVELEDIT_CONFIG_INI")).trimmed();
    if (!env_openw3d.isEmpty()) {
        return env_openw3d;
    }

    const QString env_legacy =
        QString::fromLocal8Bit(qgetenv("LEVELEDIT_CONFIG_INI")).trimmed();
    if (!env_legacy.isEmpty()) {
        return env_legacy;
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("LevelEdit.ini"));
}

QSettings OpenLevelEditSettings()
{
    return QSettings(LevelEditSettingsPath(), QSettings::IniFormat);
}

} // namespace leveledit_qt
