#include "FeatureProfile.h"
#include "ConfigSettings.h"

#include <QString>

namespace leveledit_qt {

namespace {

LevelEditQtProfile BuildDefaultProfile()
{
#ifdef LEVELEDIT_QT_PROFILE_FULL
    return LevelEditQtProfile::Full;
#else
    return LevelEditQtProfile::Public;
#endif
}

} // namespace

LevelEditQtProfile ParseProfileFromBuildOrSettings()
{
    const LevelEditQtProfile default_profile = BuildDefaultProfile();

    QSettings settings = OpenLevelEditSettings();
    const QString configured_profile =
        settings.value(QStringLiteral("LevelEditQt/Profile"), QString()).toString().trimmed().toLower();

    if (configured_profile == QStringLiteral("public")) {
        return LevelEditQtProfile::Public;
    }

    if (configured_profile == QStringLiteral("full")) {
        return LevelEditQtProfile::Full;
    }

    return default_profile;
}

const char *ToString(LevelEditQtProfile profile)
{
    return profile == LevelEditQtProfile::Full ? "full" : "public";
}

bool IsFullProfile(LevelEditQtProfile profile)
{
    return profile == LevelEditQtProfile::Full;
}

} // namespace leveledit_qt
