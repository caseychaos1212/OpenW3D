#pragma once

#include <QDialog>

#include "sphereobj.h"

class QDoubleSpinBox;
class QSlider;
class QSpinBox;

class OpacityVectorEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit OpacityVectorEditDialog(const AlphaVectorStruct &value, QWidget *parent = nullptr);

    AlphaVectorStruct value() const;

private slots:
    void handleIntensitySlider(int value);
    void handleIntensitySpin(double value);
    void handleAngleChanged();

private:
    void syncFromValue();
    float sliderPositionFromIntensity(float intensity) const;
    float intensityFromSliderPosition(float position) const;
    void updateValueFromControls();

    AlphaVectorStruct _value;

    QSlider *_intensitySlider = nullptr;
    QDoubleSpinBox *_intensitySpin = nullptr;
    QSpinBox *_angleYSpin = nullptr;
    QSpinBox *_angleZSpin = nullptr;
};
