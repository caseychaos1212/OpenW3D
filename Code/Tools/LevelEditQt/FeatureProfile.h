#pragma once

namespace leveledit_qt {

enum class LevelEditQtProfile {
    Public,
    Full,
};

LevelEditQtProfile ParseProfileFromBuildOrSettings();
const char *ToString(LevelEditQtProfile profile);
bool IsFullProfile(LevelEditQtProfile profile);

} // namespace leveledit_qt
