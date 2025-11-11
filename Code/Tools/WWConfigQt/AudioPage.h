#pragma once

#include <QWidget>

#include "WWConfigBackend.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

class AudioPage : public QWidget
{
    Q_OBJECT

public:
    explicit AudioPage(WWConfigBackend &backend, QWidget *parent = nullptr);

    void refresh();

private:
    void buildUi();
    void updateFromSettings();
    void setVolumeRow(QSlider *slider, QCheckBox *check, float value, bool enabled);

    WWConfigBackend &m_backend;
    AudioSettings m_settings;

    QComboBox *m_driverCombo = nullptr;
    QCheckBox *m_stereoCheck = nullptr;
    QComboBox *m_qualityCombo = nullptr;
    QComboBox *m_rateCombo = nullptr;
    QComboBox *m_speakerCombo = nullptr;

    QCheckBox *m_soundEnableCheck = nullptr;
    QCheckBox *m_musicEnableCheck = nullptr;
    QCheckBox *m_dialogEnableCheck = nullptr;
    QCheckBox *m_cinematicEnableCheck = nullptr;

    QSlider *m_soundSlider = nullptr;
    QSlider *m_musicSlider = nullptr;
    QSlider *m_dialogSlider = nullptr;
    QSlider *m_cinematicSlider = nullptr;
};
