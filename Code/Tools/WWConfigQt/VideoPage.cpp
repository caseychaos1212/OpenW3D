#include "VideoPage.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace
{
QString AdapterDisplayName(const VideoAdapterInfo &adapter)
{
    if (!adapter.description.empty()) {
        return QString::fromStdString(adapter.description);
    }
    if (!adapter.deviceName.empty()) {
        return QString::fromStdString(adapter.deviceName);
    }
    return QObject::tr("Display Adapter");
}
} // namespace

VideoPage::VideoPage(WWConfigBackend &backend, QWidget *parent)
    : QWidget(parent),
      m_backend(backend)
{
    buildUi();
    refresh();
}

void VideoPage::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto *deviceGroup = new QGroupBox(tr("Device"), this);
    auto *deviceLayout = new QFormLayout(deviceGroup);

    m_driverCombo = new QComboBox(deviceGroup);
    deviceLayout->addRow(tr("Adapter"), m_driverCombo);

    m_bitDepthCombo = new QComboBox(deviceGroup);
    deviceLayout->addRow(tr("Color Depth"), m_bitDepthCombo);

    m_windowedCheck = new QCheckBox(tr("Run in window"), deviceGroup);
    deviceLayout->addRow(QString(), m_windowedCheck);

    layout->addWidget(deviceGroup);

    auto *resolutionGroup = new QGroupBox(tr("Resolution"), this);
    auto *resolutionLayout = new QFormLayout(resolutionGroup);

    m_resolutionSlider = new QSlider(Qt::Horizontal, resolutionGroup);
    m_resolutionSlider->setRange(0, 0);
    resolutionLayout->addRow(tr("Preset"), m_resolutionSlider);

    m_resolutionValue = new QLabel(tr("N/A"), resolutionGroup);
    resolutionLayout->addRow(tr("Current"), m_resolutionValue);

    m_textureDepthLabel = new QLabel(tr("Texture depth: --"), resolutionGroup);
    resolutionLayout->addRow(QString(), m_textureDepthLabel);

    layout->addWidget(resolutionGroup);

    auto *note = new QLabel(tr("Pick any adapter and resolution combination. Changes are written to "
                               "the same registry keys that the classic WWConfig dialog edits, so the "
                               "game and the MFC tool will see them immediately."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();

    connect(m_driverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_blockSignals) {
            return;
        }
        updateBitDepthOptions();
        updateResolutionSlider();
        applySelection();
        updateControlStates();
    });

    connect(m_bitDepthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_blockSignals) {
            return;
        }
        m_currentBitDepth = m_bitDepthCombo->currentData().toInt();
        updateResolutionSlider();
        applySelection();
    });

    connect(m_resolutionSlider, &QSlider::valueChanged, this, [this](int) {
        if (m_blockSignals) {
            return;
        }
        updateResolutionLabel();
        applySelection();
    });

    connect(m_windowedCheck, &QCheckBox::toggled, this, [this](bool) {
        if (m_blockSignals) {
            return;
        }
        applySelection();
    });
}

void VideoPage::refresh()
{
    m_blockSignals = true;
    m_settings = m_backend.loadVideoSettings();
    m_adapters = m_backend.enumerateVideoAdapters();
    populateDrivers();
    updateBitDepthOptions();
    updateResolutionSlider();
    m_windowedCheck->setChecked(m_settings.windowed);
    m_textureDepthLabel->setText(tr("Texture depth: %1-bit")
                                     .arg(m_settings.textureDepth > 0 ? m_settings.textureDepth : 0));
    updateControlStates();
    m_blockSignals = false;
    updateResolutionLabel();
    applySelection(false);
}

void VideoPage::populateDrivers()
{
    m_driverCombo->clear();

    if (m_adapters.empty()) {
        m_driverCombo->addItem(tr("No adapters detected"));
        return;
    }

    const QString desired = QString::fromStdString(m_settings.deviceName);
    int selectedIndex = 0;
    for (int i = 0; i < static_cast<int>(m_adapters.size()); ++i) {
        const QString label = AdapterDisplayName(m_adapters[i]);
        m_driverCombo->addItem(label, i);
        if (!desired.isEmpty()) {
            const QString adapterName = QString::fromStdString(m_adapters[i].deviceName);
            if (adapterName.compare(desired, Qt::CaseInsensitive) == 0) {
                selectedIndex = i;
            }
        }
    }

    m_driverCombo->setCurrentIndex(selectedIndex);
}

void VideoPage::updateBitDepthOptions()
{
    m_bitDepthCombo->clear();

    const VideoAdapterInfo *adapter = currentAdapter();
    if (!adapter) {
        m_currentBitDepth = (m_settings.bitDepth > 0) ? m_settings.bitDepth : 32;
        return;
    }

    std::vector<int> depths;
    depths.reserve(adapter->resolutions.size());
    for (const VideoResolution &mode : adapter->resolutions) {
        if (std::find(depths.begin(), depths.end(), mode.bitDepth) == depths.end()) {
            depths.push_back(mode.bitDepth);
        }
    }
    std::sort(depths.begin(), depths.end());

    if (depths.empty()) {
        m_currentBitDepth = (m_settings.bitDepth > 0) ? m_settings.bitDepth : 32;
        return;
    }

    int targetDepth = (m_settings.bitDepth > 0) ? m_settings.bitDepth : depths.front();
    if (std::find(depths.begin(), depths.end(), targetDepth) == depths.end()) {
        targetDepth = depths.front();
    }

    for (int depth : depths) {
        m_bitDepthCombo->addItem(tr("%1-bit color").arg(depth), depth);
        if (depth == targetDepth) {
            m_bitDepthCombo->setCurrentIndex(m_bitDepthCombo->count() - 1);
        }
    }
    m_currentBitDepth = targetDepth;
}

void VideoPage::updateResolutionSlider()
{
    m_activeResolutions.clear();
    const VideoAdapterInfo *adapter = currentAdapter();

    if (!adapter) {
        m_resolutionSlider->setRange(0, 0);
        m_resolutionSlider->setEnabled(false);
        m_resolutionValue->setText(tr("N/A"));
        return;
    }

    for (const VideoResolution &mode : adapter->resolutions) {
        if (mode.bitDepth == m_currentBitDepth) {
            m_activeResolutions.push_back(mode);
        }
    }

    if (m_activeResolutions.empty()) {
        m_resolutionSlider->setRange(0, 0);
        m_resolutionSlider->setEnabled(false);
        m_resolutionValue->setText(tr("N/A"));
        return;
    }

    const int maxIndex = static_cast<int>(m_activeResolutions.size()) - 1;

    int selectedIndex = 0;
    const auto matchIt = std::find_if(
        m_activeResolutions.begin(), m_activeResolutions.end(), [&](const VideoResolution &mode) {
            return mode.width == m_settings.width && mode.height == m_settings.height;
        });
    if (matchIt != m_activeResolutions.end()) {
        selectedIndex = static_cast<int>(std::distance(m_activeResolutions.begin(), matchIt));
    } else if (m_settings.width > 0 && m_settings.height > 0) {
        int bestError = std::numeric_limits<int>::max();
        for (int i = 0; i <= maxIndex; ++i) {
            const VideoResolution &mode = m_activeResolutions[i];
            const int error = std::abs(mode.width - m_settings.width) + std::abs(mode.height - m_settings.height);
            if (error < bestError) {
                bestError = error;
                selectedIndex = i;
            }
        }
    }

    m_resolutionSlider->setEnabled(true);
    m_blockSignals = true;
    m_resolutionSlider->setRange(0, maxIndex);
    m_resolutionSlider->setValue(selectedIndex);
    m_blockSignals = false;
    updateResolutionLabel();
}

void VideoPage::updateResolutionLabel()
{
    if (m_activeResolutions.empty()) {
        m_resolutionValue->setText(tr("N/A"));
        return;
    }

    const int index = std::clamp(
        m_resolutionSlider->value(), 0, static_cast<int>(m_activeResolutions.size()) - 1);
    const VideoResolution &mode = m_activeResolutions[index];
    m_resolutionValue->setText(tr("%1 x %2").arg(mode.width).arg(mode.height));
}

void VideoPage::applySelection(bool persist)
{
    if (m_activeResolutions.empty()) {
        return;
    }

    const VideoAdapterInfo *adapter = currentAdapter();
    if (!adapter) {
        return;
    }

    const int index = std::clamp(
        m_resolutionSlider->value(), 0, static_cast<int>(m_activeResolutions.size()) - 1);
    const VideoResolution &mode = m_activeResolutions[index];

    m_settings.deviceName = adapter->deviceName;
    m_settings.width = mode.width;
    m_settings.height = mode.height;
    m_settings.bitDepth = mode.bitDepth;
    m_settings.windowed = m_windowedCheck->isChecked();

    if (m_settings.textureDepth <= 0) {
        m_settings.textureDepth = mode.bitDepth;
    }
    m_textureDepthLabel->setText(tr("Texture depth: %1-bit").arg(m_settings.textureDepth));

    if (persist) {
        m_backend.saveVideoSettings(m_settings);
    }
}

void VideoPage::updateControlStates()
{
    const bool hasAdapters = !m_adapters.empty();
    m_driverCombo->setEnabled(hasAdapters);
    m_bitDepthCombo->setEnabled(hasAdapters && m_bitDepthCombo->count() > 0);
    m_windowedCheck->setEnabled(hasAdapters);
    m_resolutionSlider->setEnabled(hasAdapters && !m_activeResolutions.empty());
}

const VideoAdapterInfo *VideoPage::currentAdapter() const
{
    if (m_adapters.empty()) {
        return nullptr;
    }

    const int row = m_driverCombo->currentIndex();
    if (row < 0 || row >= static_cast<int>(m_adapters.size())) {
        return nullptr;
    }
    return &m_adapters[row];
}
