#include "AudioPage.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace
{
QString DeviceDisplayName(const AudioSettings &settings)
{
    if (settings.deviceName.empty()) {
        return QObject::tr("Default Device");
    }
    return QString::fromStdString(settings.deviceName);
}

int RateIndexFromSampleRate(int sampleRate)
{
    switch (sampleRate) {
    case 11025: return 0;
    case 22050: return 1;
    default: return 2;
    }
}
} // namespace

AudioPage::AudioPage(WWConfigBackend &backend, QWidget *parent)
    : QWidget(parent),
      m_backend(backend)
{
    buildUi();
    refresh();
}

void AudioPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto *deviceGroup = new QGroupBox(tr("Device"), this);
    auto *deviceLayout = new QFormLayout(deviceGroup);

    m_driverCombo = new QComboBox(deviceGroup);
    m_driverCombo->setEnabled(false);
    deviceLayout->addRow(tr("Driver"), m_driverCombo);

    m_stereoCheck = new QCheckBox(tr("Stereo playback"), deviceGroup);
    m_stereoCheck->setEnabled(false);
    deviceLayout->addRow(QString(), m_stereoCheck);

    layout->addWidget(deviceGroup);

    auto *playbackGroup = new QGroupBox(tr("Playback"), this);
    auto *playbackLayout = new QFormLayout(playbackGroup);

    m_qualityCombo = new QComboBox(playbackGroup);
    m_qualityCombo->addItems({tr("8-bit"), tr("16-bit")});
    m_qualityCombo->setEnabled(false);
    playbackLayout->addRow(tr("Quality"), m_qualityCombo);

    m_rateCombo = new QComboBox(playbackGroup);
    m_rateCombo->addItems({tr("11 kHz"), tr("22 kHz"), tr("44 kHz")});
    m_rateCombo->setEnabled(false);
    playbackLayout->addRow(tr("Sample Rate"), m_rateCombo);

    m_speakerCombo = new QComboBox(playbackGroup);
    m_speakerCombo->addItems({tr("2 Speaker"), tr("Headphones"), tr("Surround"), tr("4 Speaker")});
    m_speakerCombo->setEnabled(false);
    playbackLayout->addRow(tr("Speaker Setup"), m_speakerCombo);

    layout->addWidget(playbackGroup);

    auto *volumeGroup = new QGroupBox(tr("Volume"), this);
    auto *volumeLayout = new QGridLayout(volumeGroup);

    auto createRow = [&](int row, const QString &labelText, QCheckBox *&check, QSlider *&slider) {
        check = new QCheckBox(labelText, volumeGroup);
        check->setEnabled(false);
        slider = new QSlider(Qt::Horizontal, volumeGroup);
        slider->setRange(0, 100);
        slider->setEnabled(false);
        volumeLayout->addWidget(check, row, 0);
        volumeLayout->addWidget(slider, row, 1);
    };

    createRow(0, tr("Sound Effects"), m_soundEnableCheck, m_soundSlider);
    createRow(1, tr("Music"), m_musicEnableCheck, m_musicSlider);
    createRow(2, tr("Dialog"), m_dialogEnableCheck, m_dialogSlider);
    createRow(3, tr("Cinematic"), m_cinematicEnableCheck, m_cinematicSlider);

    layout->addWidget(volumeGroup);

    auto *note = new QLabel(tr("Audio controls mirror the original WWConfig layout. They are locked "
                               "while we finish extracting the backend logic; current registry "
                               "values are shown so we can verify the data flow."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
}

void AudioPage::refresh()
{
    m_settings = m_backend.loadAudioSettings();
    updateFromSettings();
}

void AudioPage::updateFromSettings()
{
    m_driverCombo->clear();
    m_driverCombo->addItem(DeviceDisplayName(m_settings));
    m_driverCombo->setCurrentIndex(0);

    m_stereoCheck->setChecked(m_settings.stereo);

    const int qualityIndex = (m_settings.bitDepth <= 8) ? 0 : 1;
    m_qualityCombo->setCurrentIndex(qualityIndex);
    m_rateCombo->setCurrentIndex(RateIndexFromSampleRate(m_settings.sampleRate));
    m_speakerCombo->setCurrentIndex(std::clamp(m_settings.speakerType, 0, 3));

    setVolumeRow(m_soundSlider, m_soundEnableCheck, m_settings.soundVolume, m_settings.soundEnabled);
    setVolumeRow(m_musicSlider, m_musicEnableCheck, m_settings.musicVolume, m_settings.musicEnabled);
    setVolumeRow(m_dialogSlider, m_dialogEnableCheck, m_settings.dialogVolume, m_settings.dialogEnabled);
    setVolumeRow(m_cinematicSlider, m_cinematicEnableCheck, m_settings.cinematicVolume, m_settings.cinematicEnabled);
}

void AudioPage::setVolumeRow(QSlider *slider, QCheckBox *check, float value, bool enabled)
{
    slider->setValue(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100.0f)));
    check->setChecked(enabled);
}
