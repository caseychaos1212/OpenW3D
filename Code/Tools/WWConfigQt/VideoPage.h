#pragma once

#include <QWidget>

#include <vector>

#include "WWConfigBackend.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

class VideoPage : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPage(WWConfigBackend &backend, QWidget *parent = nullptr);

    void refresh();

private:
    void buildUi();
    void populateDrivers();
    void updateBitDepthOptions();
    void updateResolutionSlider();
    void updateResolutionLabel();
    void applySelection(bool persist = true);
    void updateControlStates();

    const VideoAdapterInfo *currentAdapter() const;

    WWConfigBackend &m_backend;
    VideoSettings m_settings;
    std::vector<VideoAdapterInfo> m_adapters;
    std::vector<VideoResolution> m_activeResolutions;
    int m_currentBitDepth = 32;
    bool m_blockSignals = false;

    QComboBox *m_driverCombo = nullptr;
    QComboBox *m_bitDepthCombo = nullptr;
    QSlider *m_resolutionSlider = nullptr;
    QLabel *m_resolutionValue = nullptr;
    QCheckBox *m_windowedCheck = nullptr;
    QLabel *m_textureDepthLabel = nullptr;
};
