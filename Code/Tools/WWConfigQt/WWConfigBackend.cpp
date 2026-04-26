#include "WWConfigBackend.h"

#include "../WWConfig/Locale_API.h"
#include "../WWConfig/WWConfigSettings.h"

WWConfigBackend::WWConfigBackend() = default;

WWConfigBackend::~WWConfigBackend()
{
    if (m_localeInitialized) {
        Locale_Restore();
        m_localeInitialized = false;
    }
}

bool WWConfigBackend::initializeLocale(int languageOverride)
{
    if (m_localeInitialized) {
        Locale_Restore();
        m_localeInitialized = false;
    }

    int language = languageOverride;
    if (Locale_Init(language, "WWConfig.loc")) {
        m_localeInitialized = true;
        return true;
    }

    return false;
}

QString WWConfigBackend::localizedString(int stringId) const
{
    if (!m_localeInitialized) {
        return {};
    }

    const wchar_t *text = Locale_GetString(stringId);
    if (!text) {
        return {};
    }

    return QString::fromWCharArray(text);
}

RenderSettings WWConfigBackend::loadRenderSettings() const
{
    RenderSettings settings;
    WWConfig::LoadRenderSettings(settings);
    return settings;
}

bool WWConfigBackend::saveRenderSettings(const RenderSettings &settings)
{
    return WWConfig::SaveRenderSettings(settings);
}

void WWConfigBackend::autoConfigRenderSettings()
{
    WWConfig::AutoConfigRenderSettings();
}

VideoSettings WWConfigBackend::loadVideoSettings() const
{
    VideoSettings settings;
    WWConfig::LoadVideoSettings(settings);
    return settings;
}

bool WWConfigBackend::saveVideoSettings(const VideoSettings &settings)
{
    return WWConfig::SaveVideoSettings(settings);
}

AudioSettings WWConfigBackend::loadAudioSettings() const
{
    AudioSettings settings;
    WWConfig::LoadAudioSettings(settings);
    return settings;
}

bool WWConfigBackend::saveAudioSettings(const AudioSettings &settings)
{
    return WWConfig::SaveAudioSettings(settings);
}

std::vector<VideoAdapterInfo> WWConfigBackend::enumerateVideoAdapters() const
{
    std::vector<VideoAdapterInfo> adapters;
    WWConfig::EnumerateVideoAdapters(adapters);
    return adapters;
}
