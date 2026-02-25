#pragma once

#include "FeatureProfile.h"

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
using HWND = void *;
#endif

namespace leveledit_qt {

struct RuntimeCapabilities {
    bool source_control_enabled = false;
    bool source_control_read_only = true;
    bool ddb_json_mirror_enabled = false;
};

struct RuntimeInitOptions {
    HWND viewport_hwnd = nullptr;
    int device_index = 0;
    int bits_per_pixel = 32;
    bool windowed = true;
    LevelEditQtProfile profile = LevelEditQtProfile::Public;
};

class RuntimeSession
{
public:
    bool initialize(const RuntimeInitOptions &options, std::string &error);
    void shutdown();

    bool openLevel(const std::string &path, std::string &error);
    bool saveLevel(const std::string &path, std::string &error);
    bool executeLegacyCommand(int legacy_command_id, std::string &error);

    bool isInitialized() const { return _initialized; }
    const std::string &currentLevelPath() const { return _currentLevelPath; }
    RuntimeCapabilities capabilities() const { return _capabilities; }
    LevelEditQtProfile profile() const { return _profile; }

private:
    bool _initialized = false;
    RuntimeInitOptions _options{};
    std::string _currentLevelPath;
    RuntimeCapabilities _capabilities{};
    LevelEditQtProfile _profile = LevelEditQtProfile::Public;
};

} // namespace leveledit_qt
