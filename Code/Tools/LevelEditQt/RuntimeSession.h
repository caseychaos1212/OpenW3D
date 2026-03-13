#pragma once

#include "FeatureProfile.h"

#include <cstdint>
#include <string>
#include <vector>

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
    std::string asset_tree_path;
};

struct PresetRecord {
    std::uint32_t id = 0;
    std::uint32_t definition_id = 0;
    std::uint32_t class_id = 0;
    std::uint32_t parent_id = 0;
    bool temporary = false;
    std::string name;
};

struct PresetDefinitionField {
    std::uint32_t chunk_id = 0;
    std::uint32_t micro_id = 0;
    std::uint32_t byte_length = 0;
    std::string chunk_path;
    std::string chunk_name;
    std::string field_name;
    std::string decoded_value;
    std::string raw_hex;
};

struct PresetDefinitionDetails {
    std::uint32_t definition_id = 0;
    std::uint32_t class_id = 0;
    std::uint32_t definition_chunk_id = 0;
    std::string name;
    std::string source_path;
    std::string annotation_source_file;
    std::string annotation_class_name;
    std::uint32_t annotation_field_count = 0;
    std::vector<PresetDefinitionField> fields;
};

class RuntimeSession
{
public:
    bool initialize(const RuntimeInitOptions &options, std::string &error);
    void shutdown();

    bool openLevel(const std::string &path, std::string &error);
    bool saveLevel(const std::string &path, std::string &error);
    bool executeLegacyCommand(int legacy_command_id, std::string &error);
    bool readPresetCatalog(std::vector<PresetRecord> &records,
                           std::string &source,
                           std::string &error,
                           std::vector<std::string> *searched_paths = nullptr) const;
    bool readPresetDefinitionDetails(std::uint32_t definition_id,
                                     PresetDefinitionDetails &details,
                                     std::string &error,
                                     std::vector<std::string> *searched_paths = nullptr) const;

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
