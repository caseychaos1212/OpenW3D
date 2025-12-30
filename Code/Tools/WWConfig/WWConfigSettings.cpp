#include "WWConfigSettings.h"

#include "ini.h"
#include "registry.h"
#include "dx8caps.h"
#include "dx8wrapper.h"
#include "cpudetect.h"
#include "formconv.h"
#include "wwstring.h"
#include "openw3d.h"

#include "../../Combat/specialbuilds.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <d3d9.h>

#ifndef RENEGADE_SUB_KEY_NAME_RENDER
#if defined(FREEDEDICATEDSERVER)
#define RENEGADE_SUB_KEY_NAME_RENDER "Software\\Westwood\\RenegadeFDS\\Render"
#elif defined(MULTIPLAYERDEMO)
#define RENEGADE_SUB_KEY_NAME_RENDER "Software\\Westwood\\RenegadeMPDemo\\Render"
#elif defined(BETACLIENT)
#define RENEGADE_SUB_KEY_NAME_RENDER "Software\\Westwood\\RenegadeBeta\\Render"
#else
#define RENEGADE_SUB_KEY_NAME_RENDER "Software\\Westwood\\Renegade\\Render"
#endif
#endif

#ifndef RENEGADE_SUB_KEY_NAME_AUDIO
#if defined(FREEDEDICATEDSERVER)
#define RENEGADE_SUB_KEY_NAME_AUDIO "Software\\Westwood\\RenegadeFDS\\Sound"
#elif defined(MULTIPLAYERDEMO)
#define RENEGADE_SUB_KEY_NAME_AUDIO "Software\\Westwood\\RenegadeMPDemo\\Sound"
#elif defined(BETACLIENT)
#define RENEGADE_SUB_KEY_NAME_AUDIO "Software\\Westwood\\RenegadeBeta\\Sound"
#else
#define RENEGADE_SUB_KEY_NAME_AUDIO "Software\\Westwood\\Renegade\\Sound"
#endif
#endif

namespace
{
#if	defined(FREEDEDICATEDSERVER)
constexpr const char *kKeyNameSettings = "Software\\Westwood\\RenegadeFDS\\System Settings";
constexpr const char *kKeyNameOptions = "Software\\Westwood\\RenegadeFDS\\Options";
#elif defined(MULTIPLAYERDEMO)
constexpr const char *kKeyNameSettings = "Software\\Westwood\\RenegadeMPDemo\\System Settings";
constexpr const char *kKeyNameOptions = "Software\\Westwood\\RenegadeMPDemo\\Options";
#elif defined(BETACLIENT)
constexpr const char *kKeyNameSettings = "Software\\Westwood\\RenegadeBeta\\System Settings";
constexpr const char *kKeyNameOptions = "Software\\Westwood\\RenegadeBeta\\Options";
#else
constexpr const char *kKeyNameSettings = "Software\\Westwood\\Renegade\\System Settings";
constexpr const char *kKeyNameOptions = "Software\\Westwood\\Renegade\\Options";
#endif

// Registry value names (legacy).
constexpr const char *kRegValueNameDynLOD = "Dynamic_LOD_Budget";
constexpr const char *kRegValueNameStaticLOD = "Static_LOD_Budget";
constexpr const char *kRegValueNameDynShadows = "Dynamic_Projectors";
constexpr const char *kRegValueNameTextureFilter = "Texture_Filter_Mode";
constexpr const char *kRegValueNamePrelitMode = "Prelit_Mode";
constexpr const char *kRegValueNameShadowMode = "Shadow_Mode";
constexpr const char *kRegValueNameStaticShadows = "Static_Projectors";
constexpr const char *kRegValueNameTextureRes = "Texture_Resolution";
constexpr const char *kRegValueNameSurfaceEffect = "Surface_Effect_Detail";
constexpr const char *kRegValueNameParticleDetail = "Particle_Detail";

constexpr const char *kRegValueNameAudioStereo = "stereo";
constexpr const char *kRegValueNameAudioBits = "bits";
constexpr const char *kRegValueNameAudioHertz = "hertz";
constexpr const char *kRegValueNameAudioDevice = "device name";
constexpr const char *kRegValueNameAudioMusicEnabled = "music enabled";
constexpr const char *kRegValueNameAudioSoundEnabled = "sound enabled";
constexpr const char *kRegValueNameAudioDialogEnabled = "dialog enabled";
constexpr const char *kRegValueNameAudioCinematicEnabled = "cinematic enabled";
constexpr const char *kRegValueNameAudioMusicVolume = "music volume";
constexpr const char *kRegValueNameAudioSoundVolume = "sound volume";
constexpr const char *kRegValueNameAudioDialogVolume = "dialog volume";
constexpr const char *kRegValueNameAudioCinematicVolume = "cinematic volume";
constexpr const char *kRegValueNameAudioSpeakerType = "speaker type";

// Ini value names (new format).
constexpr const char *kIniValueNameDynLOD = "DynamicLODBudget";
constexpr const char *kIniValueNameStaticLOD = "StaticLODBudget";
constexpr const char *kIniValueNameDynShadows = "DynamicProjectors";
constexpr const char *kIniValueNameTextureFilter = "TextureFilterMode";
constexpr const char *kIniValueNamePrelitMode = "PrelitMode";
constexpr const char *kIniValueNameShadowMode = "ShadowMode";
constexpr const char *kIniValueNameStaticShadows = "StaticProjectors";
constexpr const char *kIniValueNameTextureRes = "TextureResolution";
constexpr const char *kIniValueNameSurfaceEffect = "SurfaceEffectDetail";
constexpr const char *kIniValueNameParticleDetail = "ParticleDetail";

constexpr const char *kIniValueNameAudioStereo = "Stereo";
constexpr const char *kIniValueNameAudioBits = "Bits";
constexpr const char *kIniValueNameAudioHertz = "Hertz";
constexpr const char *kIniValueNameAudioDevice = "DeviceName";
constexpr const char *kIniValueNameAudioMusicEnabled = "MusicEnabled";
constexpr const char *kIniValueNameAudioSoundEnabled = "SoundEnabled";
constexpr const char *kIniValueNameAudioDialogEnabled = "DialogEnabled";
constexpr const char *kIniValueNameAudioCinematicEnabled = "CinematicEnabled";
constexpr const char *kIniValueNameAudioMusicVolume = "MusicVolume";
constexpr const char *kIniValueNameAudioSoundVolume = "SoundVolume";
constexpr const char *kIniValueNameAudioDialogVolume = "DialogVolume";
constexpr const char *kIniValueNameAudioCinematicVolume = "CinematicVolume";
constexpr const char *kIniValueNameAudioSpeakerType = "SpeakerType";
constexpr const char *kValueNameDriverWarningDisabled = "DriverVersionCheckDisabled";

constexpr int kVolumeScale = 100;

bool SaveIni(INIClass &ini)
{
    const std::string path = WWConfig::GetConfigFilePath();
    return ini.Save(path.c_str()) >= 0;
}

bool LoadRenderSettingsFromIni(INIClass &ini, RenderSettings &settings)
{
    if (!ini.Is_Loaded() || !ini.Section_Present(W3D_SECTION_SYSTEM)) {
        return false;
    }

    settings.dynamicLOD = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameDynLOD, settings.dynamicLOD);
    settings.staticLOD = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameStaticLOD, settings.staticLOD);
    settings.dynamicShadows = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameDynShadows, settings.dynamicShadows);
    settings.staticShadows = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameStaticShadows, settings.staticShadows);
    settings.prelitMode = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNamePrelitMode, settings.prelitMode);
    settings.textureFilter = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameTextureFilter, settings.textureFilter);
    settings.shadowMode = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameShadowMode, settings.shadowMode);
    settings.textureResolution = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameTextureRes, settings.textureResolution);
    settings.surfaceEffect = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameSurfaceEffect, settings.surfaceEffect);
    settings.particleDetail = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNameParticleDetail, settings.particleDetail);
    settings.lightingMode = ini.Get_Int(W3D_SECTION_SYSTEM, kIniValueNamePrelitMode, settings.lightingMode);

    return true;
}

bool LoadRenderSettingsFromRegistry(RenderSettings &settings)
{
    RegistryClass registry(kKeyNameSettings);
    if (!registry.Is_Valid()) {
        return false;
    }

    settings.dynamicLOD = registry.Get_Int(kRegValueNameDynLOD, settings.dynamicLOD);
    settings.staticLOD = registry.Get_Int(kRegValueNameStaticLOD, settings.staticLOD);
    settings.dynamicShadows = registry.Get_Int(kRegValueNameDynShadows, settings.dynamicShadows);
    settings.staticShadows = registry.Get_Int(kRegValueNameStaticShadows, settings.staticShadows);
    settings.prelitMode = registry.Get_Int(kRegValueNamePrelitMode, settings.prelitMode);
    settings.textureFilter = registry.Get_Int(kRegValueNameTextureFilter, settings.textureFilter);
    settings.shadowMode = registry.Get_Int(kRegValueNameShadowMode, settings.shadowMode);
    settings.textureResolution = registry.Get_Int(kRegValueNameTextureRes, settings.textureResolution);
    settings.surfaceEffect = registry.Get_Int(kRegValueNameSurfaceEffect, settings.surfaceEffect);
    settings.particleDetail = registry.Get_Int(kRegValueNameParticleDetail, settings.particleDetail);
    settings.lightingMode = registry.Get_Int(kRegValueNamePrelitMode, settings.lightingMode);

    return true;
}

void SaveRenderSettingsToIni(const RenderSettings &settings, INIClass &ini)
{
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameDynLOD, settings.dynamicLOD);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameStaticLOD, settings.staticLOD);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameDynShadows, settings.dynamicShadows);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameStaticShadows, settings.staticShadows);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNamePrelitMode, settings.prelitMode);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameTextureFilter, settings.textureFilter);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameShadowMode, settings.shadowMode);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameTextureRes, settings.textureResolution);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameSurfaceEffect, settings.surfaceEffect);
    ini.Put_Int(W3D_SECTION_SYSTEM, kIniValueNameParticleDetail, settings.particleDetail);
}

bool SaveRenderSettingsToRegistry(const RenderSettings &settings)
{
    RegistryClass registry(kKeyNameSettings);
    if (!registry.Is_Valid()) {
        return false;
    }

    registry.Set_Int(kRegValueNameDynLOD, settings.dynamicLOD);
    registry.Set_Int(kRegValueNameStaticLOD, settings.staticLOD);
    registry.Set_Int(kRegValueNameDynShadows, settings.dynamicShadows);
    registry.Set_Int(kRegValueNameStaticShadows, settings.staticShadows);
    registry.Set_Int(kRegValueNamePrelitMode, settings.prelitMode);
    registry.Set_Int(kRegValueNameTextureFilter, settings.textureFilter);
    registry.Set_Int(kRegValueNameShadowMode, settings.shadowMode);
    registry.Set_Int(kRegValueNameTextureRes, settings.textureResolution);
    registry.Set_Int(kRegValueNameSurfaceEffect, settings.surfaceEffect);
    registry.Set_Int(kRegValueNameParticleDetail, settings.particleDetail);

    return true;
}

bool LoadVideoSettingsFromIni(INIClass &ini, VideoSettings &settings)
{
    if (!ini.Is_Loaded() || !ini.Section_Present(W3D_SECTION_RENDER)) {
        return false;
    }

    char deviceName[256] = {};
    ini.Get_String(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_NAME, "", deviceName, sizeof(deviceName));
    if (deviceName[0] != '\0') {
        settings.deviceName = deviceName;
    }

    const int width = ini.Get_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_WIDTH, -1);
    const int height = ini.Get_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_HEIGHT, -1);
    const int bitDepth = ini.Get_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_DEPTH, -1);
    const int windowed = ini.Get_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_WINDOWED, settings.windowed ? 1 : 0);
    const int textureDepth = ini.Get_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_TEXTURE_DEPTH, -1);

    if (width > 0) {
        settings.width = width;
    }
    if (height > 0) {
        settings.height = height;
    }
    if (bitDepth > 0) {
        settings.bitDepth = bitDepth;
    }
    settings.windowed = windowed != 0;
    if (textureDepth > 0) {
        settings.textureDepth = textureDepth;
    }

    return true;
}

bool LoadVideoSettingsFromRegistry(VideoSettings &settings)
{
    RegistryClass registry(RENEGADE_SUB_KEY_NAME_RENDER);
    if (!registry.Is_Valid()) {
        return false;
    }

    char deviceName[256] = {};
    registry.Get_String(VALUE_NAME_RENDER_DEVICE_NAME, deviceName, sizeof(deviceName));
    if (deviceName[0] != '\0') {
        settings.deviceName = deviceName;
    }

    const int width = registry.Get_Int(VALUE_NAME_RENDER_DEVICE_WIDTH, -1);
    const int height = registry.Get_Int(VALUE_NAME_RENDER_DEVICE_HEIGHT, -1);
    const int bitDepth = registry.Get_Int(VALUE_NAME_RENDER_DEVICE_DEPTH, -1);
    const int windowed = registry.Get_Int(VALUE_NAME_RENDER_DEVICE_WINDOWED, settings.windowed ? 1 : 0);
    const int textureDepth = registry.Get_Int(VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH, -1);

    if (width > 0) {
        settings.width = width;
    }
    if (height > 0) {
        settings.height = height;
    }
    if (bitDepth > 0) {
        settings.bitDepth = bitDepth;
    }
    settings.windowed = windowed != 0;
    if (textureDepth > 0) {
        settings.textureDepth = textureDepth;
    }

    return true;
}

void SaveVideoSettingsToIni(const VideoSettings &settings, INIClass &ini)
{
    ini.Put_String(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_NAME, settings.deviceName.c_str());
    ini.Put_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_WIDTH, settings.width);
    ini.Put_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_HEIGHT, settings.height);
    ini.Put_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_DEPTH, settings.bitDepth);
    ini.Put_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_WINDOWED, settings.windowed ? 1 : 0);
    ini.Put_Int(W3D_SECTION_RENDER, VALUE_INI_RENDER_DEVICE_TEXTURE_DEPTH, settings.textureDepth);
}

bool SaveVideoSettingsToRegistry(const VideoSettings &settings)
{
    RegistryClass registry(RENEGADE_SUB_KEY_NAME_RENDER);
    if (!registry.Is_Valid()) {
        return false;
    }

    registry.Set_String(VALUE_NAME_RENDER_DEVICE_NAME, settings.deviceName.c_str());
    registry.Set_Int(VALUE_NAME_RENDER_DEVICE_WIDTH, settings.width);
    registry.Set_Int(VALUE_NAME_RENDER_DEVICE_HEIGHT, settings.height);
    registry.Set_Int(VALUE_NAME_RENDER_DEVICE_DEPTH, settings.bitDepth);
    registry.Set_Int(VALUE_NAME_RENDER_DEVICE_WINDOWED, settings.windowed ? 1 : 0);
    registry.Set_Int(VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH, settings.textureDepth);
    return true;
}

bool LoadAudioSettingsFromIni(INIClass &ini, AudioSettings &settings)
{
    if (!ini.Is_Loaded() || !ini.Section_Present(W3D_SECTION_SOUND)) {
        return false;
    }

    char deviceName[256] = {};
    ini.Get_String(W3D_SECTION_SOUND, kIniValueNameAudioDevice, "", deviceName, sizeof(deviceName));
    if (deviceName[0] != '\0') {
        settings.deviceName = deviceName;
    }

    settings.stereo = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioStereo, settings.stereo ? 1 : 0) != 0;
    settings.bitDepth = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioBits, settings.bitDepth);
    settings.sampleRate = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioHertz, settings.sampleRate);
    settings.musicEnabled = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioMusicEnabled, settings.musicEnabled ? 1 : 0) != 0;
    settings.soundEnabled = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioSoundEnabled, settings.soundEnabled ? 1 : 0) != 0;
    settings.dialogEnabled = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioDialogEnabled, settings.dialogEnabled ? 1 : 0) != 0;
    settings.cinematicEnabled = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioCinematicEnabled, settings.cinematicEnabled ? 1 : 0) != 0;
    settings.soundVolume = std::clamp(ini.Get_Float(W3D_SECTION_SOUND, kIniValueNameAudioSoundVolume, settings.soundVolume), 0.0f, 1.0f);
    settings.musicVolume = std::clamp(ini.Get_Float(W3D_SECTION_SOUND, kIniValueNameAudioMusicVolume, settings.musicVolume), 0.0f, 1.0f);
    settings.dialogVolume = std::clamp(ini.Get_Float(W3D_SECTION_SOUND, kIniValueNameAudioDialogVolume, settings.dialogVolume), 0.0f, 1.0f);
    settings.cinematicVolume = std::clamp(ini.Get_Float(W3D_SECTION_SOUND, kIniValueNameAudioCinematicVolume, settings.cinematicVolume), 0.0f, 1.0f);
    settings.speakerType = ini.Get_Int(W3D_SECTION_SOUND, kIniValueNameAudioSpeakerType, settings.speakerType);

    return true;
}

bool LoadAudioSettingsFromRegistry(AudioSettings &settings)
{
    RegistryClass registry(RENEGADE_SUB_KEY_NAME_AUDIO);
    if (!registry.Is_Valid()) {
        return false;
    }

    char deviceName[256] = {};
    registry.Get_String(kRegValueNameAudioDevice, deviceName, sizeof(deviceName));
    if (deviceName[0] != '\0') {
        settings.deviceName = deviceName;
    }

    settings.stereo = registry.Get_Int(kRegValueNameAudioStereo, settings.stereo ? 1 : 0) != 0;
    settings.bitDepth = registry.Get_Int(kRegValueNameAudioBits, settings.bitDepth);
    settings.sampleRate = registry.Get_Int(kRegValueNameAudioHertz, settings.sampleRate);
    settings.musicEnabled = registry.Get_Int(kRegValueNameAudioMusicEnabled, settings.musicEnabled ? 1 : 0) != 0;
    settings.soundEnabled = registry.Get_Int(kRegValueNameAudioSoundEnabled, settings.soundEnabled ? 1 : 0) != 0;
    settings.dialogEnabled = registry.Get_Int(kRegValueNameAudioDialogEnabled, settings.dialogEnabled ? 1 : 0) != 0;
    settings.cinematicEnabled = registry.Get_Int(kRegValueNameAudioCinematicEnabled, settings.cinematicEnabled ? 1 : 0) != 0;
    settings.soundVolume = std::clamp(registry.Get_Int(kRegValueNameAudioSoundVolume, static_cast<int>(settings.soundVolume * kVolumeScale)) / static_cast<float>(kVolumeScale), 0.0f, 1.0f);
    settings.musicVolume = std::clamp(registry.Get_Int(kRegValueNameAudioMusicVolume, static_cast<int>(settings.musicVolume * kVolumeScale)) / static_cast<float>(kVolumeScale), 0.0f, 1.0f);
    settings.dialogVolume = std::clamp(registry.Get_Int(kRegValueNameAudioDialogVolume, static_cast<int>(settings.dialogVolume * kVolumeScale)) / static_cast<float>(kVolumeScale), 0.0f, 1.0f);
    settings.cinematicVolume = std::clamp(registry.Get_Int(kRegValueNameAudioCinematicVolume, static_cast<int>(settings.cinematicVolume * kVolumeScale)) / static_cast<float>(kVolumeScale), 0.0f, 1.0f);
    settings.speakerType = registry.Get_Int(kRegValueNameAudioSpeakerType, settings.speakerType);

    return true;
}

void SaveAudioSettingsToIni(const AudioSettings &settings, INIClass &ini)
{
    ini.Put_String(W3D_SECTION_SOUND, kIniValueNameAudioDevice, settings.deviceName.c_str());
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioStereo, settings.stereo ? 1 : 0);
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioBits, settings.bitDepth);
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioHertz, settings.sampleRate);
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioMusicEnabled, settings.musicEnabled ? 1 : 0);
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioSoundEnabled, settings.soundEnabled ? 1 : 0);
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioDialogEnabled, settings.dialogEnabled ? 1 : 0);
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioCinematicEnabled, settings.cinematicEnabled ? 1 : 0);
    ini.Put_Float(W3D_SECTION_SOUND, kIniValueNameAudioSoundVolume, std::clamp(settings.soundVolume, 0.0f, 1.0f));
    ini.Put_Float(W3D_SECTION_SOUND, kIniValueNameAudioMusicVolume, std::clamp(settings.musicVolume, 0.0f, 1.0f));
    ini.Put_Float(W3D_SECTION_SOUND, kIniValueNameAudioDialogVolume, std::clamp(settings.dialogVolume, 0.0f, 1.0f));
    ini.Put_Float(W3D_SECTION_SOUND, kIniValueNameAudioCinematicVolume, std::clamp(settings.cinematicVolume, 0.0f, 1.0f));
    ini.Put_Int(W3D_SECTION_SOUND, kIniValueNameAudioSpeakerType, settings.speakerType);
}

bool SaveAudioSettingsToRegistry(const AudioSettings &settings)
{
    RegistryClass registry(RENEGADE_SUB_KEY_NAME_AUDIO);
    if (!registry.Is_Valid()) {
        return false;
    }

    registry.Set_String(kRegValueNameAudioDevice, settings.deviceName.c_str());
    registry.Set_Int(kRegValueNameAudioStereo, settings.stereo ? 1 : 0);
    registry.Set_Int(kRegValueNameAudioBits, settings.bitDepth);
    registry.Set_Int(kRegValueNameAudioHertz, settings.sampleRate);
    registry.Set_Int(kRegValueNameAudioMusicEnabled, settings.musicEnabled ? 1 : 0);
    registry.Set_Int(kRegValueNameAudioSoundEnabled, settings.soundEnabled ? 1 : 0);
    registry.Set_Int(kRegValueNameAudioDialogEnabled, settings.dialogEnabled ? 1 : 0);
    registry.Set_Int(kRegValueNameAudioCinematicEnabled, settings.cinematicEnabled ? 1 : 0);
    registry.Set_Int(kRegValueNameAudioSoundVolume, static_cast<int>(std::clamp(settings.soundVolume, 0.0f, 1.0f) * kVolumeScale));
    registry.Set_Int(kRegValueNameAudioMusicVolume, static_cast<int>(std::clamp(settings.musicVolume, 0.0f, 1.0f) * kVolumeScale));
    registry.Set_Int(kRegValueNameAudioDialogVolume, static_cast<int>(std::clamp(settings.dialogVolume, 0.0f, 1.0f) * kVolumeScale));
    registry.Set_Int(kRegValueNameAudioCinematicVolume, static_cast<int>(std::clamp(settings.cinematicVolume, 0.0f, 1.0f) * kVolumeScale));
    registry.Set_Int(kRegValueNameAudioSpeakerType, settings.speakerType);

    return true;
}

}

namespace WWConfig
{
std::string GetConfigFilePath()
{
    if (const char *envPath = std::getenv("OPENW3D_CONFIG_INI")) {
        if (*envPath != '\0') {
            return std::string(envPath);
        }
    }

    if (const char *envPath = std::getenv("RENEGADE_CONFIG_INI")) {
        if (*envPath != '\0') {
            return std::string(envPath);
        }
    }

#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) > 0) {
        std::string path(modulePath);
        const size_t slash = path.find_last_of("\\/");
        if (slash != std::string::npos) {
            path.resize(slash + 1);
        } else {
            path.clear();
        }
        return path + W3D_CONF_FILE;
    }
#endif

    return std::string(W3D_CONF_FILE);
}

bool LoadRenderSettings(RenderSettings &settings)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    if (LoadRenderSettingsFromIni(ini, settings)) {
        return true;
    }

    return LoadRenderSettingsFromRegistry(settings);
}

bool SaveRenderSettings(const RenderSettings &settings)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    SaveRenderSettingsToIni(settings, ini);
    const bool iniSaved = SaveIni(ini);

    const bool registrySaved = SaveRenderSettingsToRegistry(settings);
    return iniSaved || registrySaved;
}

static void InitializeAdapterSelection(VideoSettings &videoSettings, IDirect3D9 **outD3D, D3DCAPS9 *outCaps, D3DADAPTER_IDENTIFIER9 *outAdapterId, D3DFORMAT &displayFormat)
{
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        *outD3D = nullptr;
        return;
    }

    int currentAdapterIndex = D3DADAPTER_DEFAULT;
    if (!videoSettings.deviceName.empty()) {
        const int adapterCount = static_cast<int>(d3d->GetAdapterCount());
        for (int adapterIndex = 0; adapterIndex < adapterCount; ++adapterIndex) {
            D3DADAPTER_IDENTIFIER9 id = {};
            if (SUCCEEDED(d3d->GetAdapterIdentifier(adapterIndex, 0, &id))) {
                if (_stricmp(videoSettings.deviceName.c_str(), id.Description) == 0) {
                    currentAdapterIndex = adapterIndex;
                    break;
                }
            }
        }
    }

    if (FAILED(d3d->GetDeviceCaps(currentAdapterIndex, D3DDEVTYPE_HAL, outCaps))) {
        d3d->Release();
        *outD3D = nullptr;
        return;
    }

    ZeroMemory(outAdapterId, sizeof(D3DADAPTER_IDENTIFIER9));
    if (FAILED(d3d->GetAdapterIdentifier(currentAdapterIndex, 0, outAdapterId))) {
        d3d->Release();
        *outD3D = nullptr;
        return;
    }

    videoSettings.deviceName = outAdapterId->Description;
    videoSettings.width = 800;
    videoSettings.height = 600;
    videoSettings.bitDepth = 16;
    videoSettings.windowed = false;
    videoSettings.textureDepth = 16;

    displayFormat = D3DFMT_R5G6B5;
    *outD3D = d3d;
}

void AutoConfigRenderSettings()
{
    RenderSettings renderSettings;
    LoadRenderSettings(renderSettings);

    VideoSettings videoSettings;
    LoadVideoSettings(videoSettings);

    IDirect3D9 *d3d = nullptr;
    D3DCAPS9 tmpCaps = {};
    const D3DCAPS9 *d3dCaps = nullptr;
    D3DADAPTER_IDENTIFIER9 adapterId = {};
    D3DFORMAT displayFormat = D3DFMT_R5G6B5;

    InitializeAdapterSelection(videoSettings, &d3d, &tmpCaps, &adapterId, displayFormat);
    if (!d3d) {
        return;
    }
    d3dCaps = &tmpCaps;

    DX8Caps caps(d3d, *d3dCaps, D3DFormat_To_WW3DFormat(displayFormat), adapterId);
    bool canDoMultiPass = caps.Can_Do_Multi_Pass();
    bool highEndProcessor = CPUDetectClass::Has_SSE_Instruction_Set();
    if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_AMD &&
        CPUDetectClass::Get_AMD_Processor() >= CPUDetectClass::AMD_PROCESSOR_ATHLON_025) {
        highEndProcessor = true;
    }

    renderSettings.textureResolution = caps.Support_DXTC() ? 0 : 1;

    if (caps.Support_TnL()) {
        renderSettings.dynamicLOD = 10000;
        renderSettings.staticLOD = 10000;
    } else if (highEndProcessor) {
        renderSettings.dynamicLOD = 5000;
        renderSettings.staticLOD = 5000;
    } else {
        renderSettings.dynamicLOD = 0;
        renderSettings.staticLOD = 0;
    }

    if (caps.Support_Render_To_Texture_Format(D3DFormat_To_WW3DFormat(displayFormat))) {
        if (caps.Support_TnL()) {
            renderSettings.shadowMode = 3;
            renderSettings.staticShadows = 1;
        } else {
            renderSettings.shadowMode = 2;
            renderSettings.staticShadows = 0;
        }
    } else {
        renderSettings.staticShadows = 0;
        renderSettings.shadowMode = highEndProcessor ? 1 : 0;
    }

    renderSettings.dynamicShadows = renderSettings.shadowMode != 0;

    if (caps.Support_TnL()) {
        renderSettings.surfaceEffect = 2;
    } else {
        renderSettings.surfaceEffect = highEndProcessor ? 1 : 0;
    }

    if (caps.Support_TnL() && highEndProcessor) {
        renderSettings.particleDetail = 2;
    } else if (caps.Support_TnL() || highEndProcessor) {
        renderSettings.particleDetail = 1;
    } else {
        renderSettings.particleDetail = 0;
    }

    if (!canDoMultiPass || CPUDetectClass::Get_Total_Physical_Memory() < 100 * 1024 * 1024) {
        renderSettings.prelitMode = 0;
    } else if (caps.Get_Max_Textures_Per_Pass() >= 2) {
        renderSettings.prelitMode = 2;
    } else {
        renderSettings.prelitMode = 1;
    }

    RegistryClass registryOptions(kKeyNameOptions);
    if (registryOptions.Is_Valid()) {
        registryOptions.Set_Int("ScreenUVBias", 1);
    }

    if (caps.Get_Vendor() == DX8Caps::VENDOR_NVIDIA) {
        switch (caps.Get_Device()) {
        case DX8Caps::DEVICE_NVIDIA_TNT2_ALADDIN:
        case DX8Caps::DEVICE_NVIDIA_TNT2:
        case DX8Caps::DEVICE_NVIDIA_TNT2_ULTRA:
        case DX8Caps::DEVICE_NVIDIA_TNT2_VANTA:
        case DX8Caps::DEVICE_NVIDIA_TNT2_M64:
        case DX8Caps::DEVICE_NVIDIA_TNT:
        case DX8Caps::DEVICE_NVIDIA_RIVA_128:
        case DX8Caps::DEVICE_NVIDIA_TNT_VANTA:
        case DX8Caps::DEVICE_NVIDIA_NV1:
            renderSettings.textureFilter = 0;
            break;
        default:
            renderSettings.textureFilter = 1;
        }
    } else if (caps.Get_Vendor() == DX8Caps::VENDOR_ATI) {
        switch (caps.Get_Device()) {
        case DX8Caps::DEVICE_ATI_RAGE_II:
        case DX8Caps::DEVICE_ATI_RAGE_II_PLUS:
        case DX8Caps::DEVICE_ATI_RAGE_IIC_PCI:
        case DX8Caps::DEVICE_ATI_RAGE_IIC_AGP:
        case DX8Caps::DEVICE_ATI_RAGE_128_MOBILITY:
        case DX8Caps::DEVICE_ATI_RAGE_128_MOBILITY_M3:
        case DX8Caps::DEVICE_ATI_RAGE_128_MOBILITY_M4:
        case DX8Caps::DEVICE_ATI_RAGE_128_PRO_ULTRA:
        case DX8Caps::DEVICE_ATI_RAGE_128_4X:
        case DX8Caps::DEVICE_ATI_RAGE_128_PRO_GL:
        case DX8Caps::DEVICE_ATI_RAGE_128_PRO_VR:
        case DX8Caps::DEVICE_ATI_RAGE_128_GL:
        case DX8Caps::DEVICE_ATI_RAGE_128_VR:
        case DX8Caps::DEVICE_ATI_RAGE_PRO:
        case DX8Caps::DEVICE_ATI_RAGE_PRO_MOBILITY:
            renderSettings.textureFilter = 0;
            if (caps.Get_Device() == DX8Caps::DEVICE_ATI_RAGE_PRO ||
                caps.Get_Device() == DX8Caps::DEVICE_ATI_RAGE_PRO_MOBILITY ||
                caps.Get_Device() == DX8Caps::DEVICE_ATI_RAGE_128_MOBILITY ||
                caps.Get_Device() == DX8Caps::DEVICE_ATI_RAGE_128_MOBILITY_M3 ||
                caps.Get_Device() == DX8Caps::DEVICE_ATI_RAGE_128_MOBILITY_M4 ||
                caps.Get_Device() == DX8Caps::DEVICE_ATI_RAGE_128_PRO_ULTRA) {
                registryOptions.Set_Int("ScreenUVBias", 0);
            }
            break;
        default:
            renderSettings.textureFilter = 1;
        }
    } else {
        renderSettings.textureFilter = 1;
    }

    if (d3d) {
        d3d->Release();
    }

    SaveVideoSettings(videoSettings);
    SaveRenderSettings(renderSettings);
}

bool LoadVideoSettings(VideoSettings &settings)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    if (LoadVideoSettingsFromIni(ini, settings)) {
        return true;
    }

    return LoadVideoSettingsFromRegistry(settings);
}

bool SaveVideoSettings(const VideoSettings &settings)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    SaveVideoSettingsToIni(settings, ini);
    const bool iniSaved = SaveIni(ini);

    const bool registrySaved = SaveVideoSettingsToRegistry(settings);
    return iniSaved || registrySaved;
}

bool LoadAudioSettings(AudioSettings &settings)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    if (LoadAudioSettingsFromIni(ini, settings)) {
        return true;
    }

    return LoadAudioSettingsFromRegistry(settings);
}

bool SaveAudioSettings(const AudioSettings &settings)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    SaveAudioSettingsToIni(settings, ini);
    const bool iniSaved = SaveIni(ini);

    const bool registrySaved = SaveAudioSettingsToRegistry(settings);
    return iniSaved || registrySaved;
}

bool IsDriverWarningDisabled()
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    const int disabled = ini.Get_Int(W3D_SECTION_RENDER, kValueNameDriverWarningDisabled, 0);
    return disabled >= 87;
}

void SetDriverWarningDisabled(bool disabled)
{
    const std::string iniPath = GetConfigFilePath();
    INIClass ini(iniPath.c_str());
    ini.Put_Int(W3D_SECTION_RENDER, kValueNameDriverWarningDisabled, disabled ? 87 : 0);
    SaveIni(ini);
}

bool EnumerateVideoAdapters(std::vector<VideoAdapterInfo> &adapters)
{
    adapters.clear();

    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        return false;
    }

    constexpr D3DFORMAT kFormats[] = {
        D3DFMT_X8R8G8B8,
        D3DFMT_A8R8G8B8,
        D3DFMT_R5G6B5,
    };

    const UINT adapterCount = d3d->GetAdapterCount();
    adapters.reserve(adapterCount);

    for (UINT adapterIndex = 0; adapterIndex < adapterCount; ++adapterIndex) {
        D3DADAPTER_IDENTIFIER9 identifier = {};
        if (FAILED(d3d->GetAdapterIdentifier(adapterIndex, 0, &identifier))) {
            continue;
        }

        VideoAdapterInfo adapterInfo;
        adapterInfo.deviceName = identifier.Description;
        adapterInfo.description = identifier.Description;

        for (D3DFORMAT format : kFormats) {
            const UINT modeCount = d3d->GetAdapterModeCount(adapterIndex, format);
            for (UINT modeIndex = 0; modeIndex < modeCount; ++modeIndex) {
                D3DDISPLAYMODE mode = {};
                if (FAILED(d3d->EnumAdapterModes(adapterIndex, format, modeIndex, &mode))) {
                    continue;
                }

                if (mode.Width < 640 || mode.Height < 480) {
                    continue;
                }

                VideoResolution resolution;
                resolution.width = static_cast<int>(mode.Width);
                resolution.height = static_cast<int>(mode.Height);
                resolution.bitDepth = (format == D3DFMT_R5G6B5) ? 16 : 32;

                const auto exists = std::find_if(adapterInfo.resolutions.begin(),
                                                 adapterInfo.resolutions.end(),
                                                 [&](const VideoResolution &existing) {
                                                     return existing.width == resolution.width &&
                                                            existing.height == resolution.height &&
                                                            existing.bitDepth == resolution.bitDepth;
                                                 });
                if (exists == adapterInfo.resolutions.end()) {
                    adapterInfo.resolutions.push_back(resolution);
                }
            }
        }

        if (adapterInfo.resolutions.empty()) {
            continue;
        }

        std::sort(adapterInfo.resolutions.begin(),
                  adapterInfo.resolutions.end(),
                  [](const VideoResolution &lhs, const VideoResolution &rhs) {
                      if (lhs.bitDepth != rhs.bitDepth) {
                          return lhs.bitDepth < rhs.bitDepth;
                      }
                      if (lhs.width != rhs.width) {
                          return lhs.width < rhs.width;
                      }
                      if (lhs.height != rhs.height) {
                          return lhs.height < rhs.height;
                      }
                      return false;
                  });

        adapters.push_back(std::move(adapterInfo));
    }

    d3d->Release();
    return !adapters.empty();
}

} // namespace WWConfig
