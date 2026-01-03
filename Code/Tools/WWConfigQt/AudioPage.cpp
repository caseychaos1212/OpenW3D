#include "AudioPage.h"

#include <algorithm>
#include <cmath>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QVBoxLayout>

#include "../WWConfig/wwconfig_ids.h"
#include "WWAudio.h"

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

int SampleRateFromIndex(int index)
{
    switch (index) {
    case 0: return 11025;
    case 1: return 22050;
    default: return 44100;
    }
}

QString LocalizedText(const WWConfigBackend &backend, int id, const QString &fallback)
{
    const QString text = backend.localizedString(id);
    return text.isEmpty() ? fallback : text;
}
} // namespace

AudioPage::AudioPage(WWConfigBackend &backend, QWidget *parent)
    : QWidget(parent),
      m_backend(backend)
{
    m_audio = WWAudioClass::Get_Instance();
    if (!m_audio) {
        m_audio = WWAudioClass::Create_Instance();
        m_ownsAudio = (m_audio != nullptr);
    }
    if (m_audio && m_ownsAudio) {
        m_audio->Initialize();
    }
    buildUi();
    refresh();
}

AudioPage::~AudioPage()
{
    if (m_audio && m_ownsAudio) {
        m_audio->Shutdown();
        delete m_audio;
    }
    m_audio = nullptr;
}

void AudioPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    auto *deviceGroup = new QGroupBox(this);
    deviceGroup->setTitle(QString());
    auto *deviceLayout = new QGridLayout(deviceGroup);
    deviceLayout->setContentsMargins(6, 6, 6, 6);
    deviceLayout->setHorizontalSpacing(6);
    deviceLayout->setVerticalSpacing(4);

    deviceLayout->addWidget(new QLabel(LocalizedText(m_backend, IDS_DRIVER, tr("Driver:")), deviceGroup), 0, 0, Qt::AlignLeft);
    m_driverList = new QListWidget(deviceGroup);
    m_driverList->setUniformItemSizes(true);
    m_driverList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_driverList->setMinimumHeight(60);
    deviceLayout->addWidget(m_driverList, 0, 1, 1, 2);

    deviceLayout->setColumnStretch(1, 1);
    layout->addWidget(deviceGroup);

    auto *volumeGroup = new QGroupBox(LocalizedText(m_backend, IDS_VOLUME, tr("Volume")), this);
    auto *volumeLayout = new QGridLayout(volumeGroup);
    volumeLayout->setContentsMargins(6, 6, 6, 6);
    volumeLayout->setHorizontalSpacing(6);
    volumeLayout->setVerticalSpacing(4);

    auto createRow = [&](int row, const QString &labelText, QCheckBox *&check, QSlider *&slider) {
        check = new QCheckBox(labelText, volumeGroup);
        slider = new QSlider(Qt::Horizontal, volumeGroup);
        slider->setRange(0, 100);
        slider->setTickInterval(10);
        volumeLayout->addWidget(check, row, 0, Qt::AlignLeft);
        volumeLayout->addWidget(slider, row, 1);
    };

    createRow(0, LocalizedText(m_backend, IDS_SOUND_EFFECTS, tr("Sound Effects")), m_soundEnableCheck, m_soundSlider);
    createRow(1, LocalizedText(m_backend, IDS_MUSIC, tr("Music")), m_musicEnableCheck, m_musicSlider);
    createRow(2, LocalizedText(m_backend, IDS_DIALOG, tr("Dialog")), m_dialogEnableCheck, m_dialogSlider);
    createRow(3, LocalizedText(m_backend, IDS_CINEMATIC, tr("Cinematic")), m_cinematicEnableCheck, m_cinematicSlider);

    volumeLayout->setColumnStretch(1, 1);
    layout->addWidget(volumeGroup);

    auto *playbackGroup = new QGroupBox(this);
    playbackGroup->setTitle(QString());
    auto *playbackLayout = new QGridLayout(playbackGroup);
    playbackLayout->setContentsMargins(6, 6, 6, 6);
    playbackLayout->setHorizontalSpacing(8);
    playbackLayout->setVerticalSpacing(4);

    m_qualityCombo = new QComboBox(playbackGroup);
    m_qualityCombo->addItems({
        LocalizedText(m_backend, IDS_8_BIT, tr("8-bit")),
        LocalizedText(m_backend, IDS_16_BIT, tr("16-bit")),
    });
    playbackLayout->addWidget(new QLabel(LocalizedText(m_backend, IDS_QUALITY, tr("Quality")), playbackGroup), 0, 0, Qt::AlignLeft);
    playbackLayout->addWidget(m_qualityCombo, 1, 0);

    m_rateCombo = new QComboBox(playbackGroup);
    m_rateCombo->addItems({
        LocalizedText(m_backend, IDS_11_KHZ, tr("11 kHz")),
        LocalizedText(m_backend, IDS_22_KHZ, tr("22 kHz")),
        LocalizedText(m_backend, IDS_44_KHZ, tr("44 kHz")),
    });
    playbackLayout->addWidget(new QLabel(LocalizedText(m_backend, IDS_PLAYBACK_RATE, tr("Playback Rate")), playbackGroup), 0, 1, Qt::AlignLeft);
    playbackLayout->addWidget(m_rateCombo, 1, 1);

    m_speakerCombo = new QComboBox(playbackGroup);
    m_speakerCombo->addItems({
        LocalizedText(m_backend, IDS_2_SPEAKER, tr("2 Speaker")),
        LocalizedText(m_backend, IDS_HEADPHONE, tr("Headphones")),
        LocalizedText(m_backend, IDS_SURROUND_SOUND, tr("Surround")),
        LocalizedText(m_backend, IDS_4_SPEAKER, tr("4 Speaker")),
    });
    playbackLayout->addWidget(new QLabel(LocalizedText(m_backend, IDS_SPEAKER_SETUP, tr("Speaker Setup")), playbackGroup), 0, 2, Qt::AlignLeft);
    playbackLayout->addWidget(m_speakerCombo, 1, 2);

    playbackLayout->setColumnStretch(0, 1);
    playbackLayout->setColumnStretch(1, 1);
    playbackLayout->setColumnStretch(2, 1);
    layout->addWidget(playbackGroup);

    m_stereoCheck = new QCheckBox(LocalizedText(m_backend, IDS_STEREO, tr("Stereo")), this);
    layout->addWidget(m_stereoCheck, 0, Qt::AlignLeft);
    layout->addStretch();

    auto applyOnChange = [this]() {
        applySettings();
    };

    connect(m_stereoCheck, &QCheckBox::toggled, this, applyOnChange);
    connect(m_qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyOnChange);
    connect(m_rateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyOnChange);
    connect(m_speakerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyOnChange);
    connect(m_soundEnableCheck, &QCheckBox::toggled, this, applyOnChange);
    connect(m_musicEnableCheck, &QCheckBox::toggled, this, applyOnChange);
    connect(m_dialogEnableCheck, &QCheckBox::toggled, this, applyOnChange);
    connect(m_cinematicEnableCheck, &QCheckBox::toggled, this, applyOnChange);
    connect(m_driverList, &QListWidget::currentRowChanged, this, applyOnChange);
    connect(m_soundSlider, &QSlider::valueChanged, this, applyOnChange);
    connect(m_musicSlider, &QSlider::valueChanged, this, applyOnChange);
    connect(m_dialogSlider, &QSlider::valueChanged, this, applyOnChange);
    connect(m_cinematicSlider, &QSlider::valueChanged, this, applyOnChange);
}

void AudioPage::refresh()
{
    m_blockSignals = true;
    m_settings = m_backend.loadAudioSettings();
    updateFromSettings();
    m_blockSignals = false;
}

void AudioPage::updateFromSettings()
{
    populateDrivers();

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

void AudioPage::applySettings()
{
    if (m_blockSignals) {
        return;
    }

    if (m_driverList) {
        QListWidgetItem *item = m_driverList->currentItem();
        if (item) {
            m_settings.deviceName = item->data(Qt::UserRole).toString().toStdString();
        }
    }

    m_settings.stereo = m_stereoCheck->isChecked();
    m_settings.bitDepth = (m_qualityCombo->currentIndex() == 0) ? 8 : 16;
    m_settings.sampleRate = SampleRateFromIndex(m_rateCombo->currentIndex());
    m_settings.speakerType = m_speakerCombo->currentIndex();

    m_settings.soundEnabled = m_soundEnableCheck->isChecked();
    m_settings.musicEnabled = m_musicEnableCheck->isChecked();
    m_settings.dialogEnabled = m_dialogEnableCheck->isChecked();
    m_settings.cinematicEnabled = m_cinematicEnableCheck->isChecked();

    m_soundSlider->setEnabled(m_settings.soundEnabled);
    m_musicSlider->setEnabled(m_settings.musicEnabled);
    m_dialogSlider->setEnabled(m_settings.dialogEnabled);
    m_cinematicSlider->setEnabled(m_settings.cinematicEnabled);

    m_settings.soundVolume = std::clamp(m_soundSlider->value() / 100.0f, 0.0f, 1.0f);
    m_settings.musicVolume = std::clamp(m_musicSlider->value() / 100.0f, 0.0f, 1.0f);
    m_settings.dialogVolume = std::clamp(m_dialogSlider->value() / 100.0f, 0.0f, 1.0f);
    m_settings.cinematicVolume = std::clamp(m_cinematicSlider->value() / 100.0f, 0.0f, 1.0f);

    m_backend.saveAudioSettings(m_settings);
}

void AudioPage::populateDrivers()
{
    m_driverList->clear();

    const QString selected = QString::fromStdString(m_settings.deviceName);
    bool selectedMatch = false;

    if (m_audio) {
        const int driverCount = m_audio->Get_3D_Device_Count();
        for (int index = 0; index < driverCount; ++index) {
            const char *driverInfo = nullptr;
            if (!m_audio->Get_3D_Device(index, &driverInfo) || !driverInfo) {
                continue;
            }
            const QString name = QString::fromLatin1(driverInfo);
            auto *item = new QListWidgetItem(name);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setData(Qt::UserRole, name);
            m_driverList->addItem(item);
            if (!selected.isEmpty() && name.compare(selected, Qt::CaseInsensitive) == 0) {
                m_driverList->setCurrentRow(m_driverList->count() - 1);
                selectedMatch = true;
            }
        }
    }

    if (m_driverList->count() == 0) {
        auto *item = new QListWidgetItem(DeviceDisplayName(m_settings));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setData(Qt::UserRole, QString());
        m_driverList->addItem(item);
        m_driverList->setCurrentRow(0);
        return;
    }

    if (!selectedMatch) {
        m_driverList->setCurrentRow(0);
    }
}

void AudioPage::setVolumeRow(QSlider *slider, QCheckBox *check, float value, bool enabled)
{
    slider->setValue(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100.0f)));
    check->setChecked(enabled);
    slider->setEnabled(enabled);
}
