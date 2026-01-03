#include "OpacityVectorEditDialog.h"

#include "euler.h"
#include "matrix3.h"
#include "quat.h"
#include "wwmath.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

void QuaternionToAngles(const Quaternion &quat, float &y_deg, float &z_deg)
{
    Matrix3D rotation = Build_Matrix3D(quat);
    EulerAnglesClass euler(rotation, EulerOrderXYZr);
    y_deg = static_cast<float>(euler.Get_Angle(1) * kRadToDeg);
    z_deg = static_cast<float>(euler.Get_Angle(2) * kRadToDeg);
    y_deg = static_cast<float>(WWMath::Wrap(y_deg, 0.0f, 360.0f));
    z_deg = static_cast<float>(WWMath::Wrap(z_deg, 0.0f, 360.0f));
}
}

OpacityVectorEditDialog::OpacityVectorEditDialog(const AlphaVectorStruct &value, QWidget *parent)
    : QDialog(parent),
      _value(value)
{
    setWindowTitle("Opacity Vector");

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    _intensitySlider = new QSlider(Qt::Horizontal, this);
    _intensitySlider->setRange(0, 100);
    form->addRow("Intensity:", _intensitySlider);

    _intensitySpin = new QDoubleSpinBox(this);
    _intensitySpin->setRange(0.0, 10.0);
    _intensitySpin->setDecimals(2);
    form->addRow("Intensity Value:", _intensitySpin);

    auto *angle_row = new QWidget(this);
    auto *angle_layout = new QHBoxLayout(angle_row);
    angle_layout->setContentsMargins(0, 0, 0, 0);

    _angleYSpin = new QSpinBox(angle_row);
    _angleYSpin->setRange(0, 179);
    _angleYSpin->setSuffix(" deg");

    _angleZSpin = new QSpinBox(angle_row);
    _angleZSpin->setRange(0, 179);
    _angleZSpin->setSuffix(" deg");

    angle_layout->addWidget(new QLabel("Y", angle_row));
    angle_layout->addWidget(_angleYSpin);
    angle_layout->addWidget(new QLabel("Z", angle_row));
    angle_layout->addWidget(_angleZSpin);

    form->addRow("Angles:", angle_row);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &OpacityVectorEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &OpacityVectorEditDialog::reject);
    layout->addWidget(buttons);

    connect(_intensitySlider, &QSlider::valueChanged, this, &OpacityVectorEditDialog::handleIntensitySlider);
    connect(_intensitySpin, &QDoubleSpinBox::valueChanged, this, &OpacityVectorEditDialog::handleIntensitySpin);
    connect(_angleYSpin, &QSpinBox::valueChanged, this, &OpacityVectorEditDialog::handleAngleChanged);
    connect(_angleZSpin, &QSpinBox::valueChanged, this, &OpacityVectorEditDialog::handleAngleChanged);

    syncFromValue();
}

AlphaVectorStruct OpacityVectorEditDialog::value() const
{
    return _value;
}

void OpacityVectorEditDialog::handleIntensitySlider(int value)
{
    const float position = static_cast<float>(value) / 10.0f;
    const float intensity = intensityFromSliderPosition(position);
    if (_intensitySpin) {
        _intensitySpin->blockSignals(true);
        _intensitySpin->setValue(intensity);
        _intensitySpin->blockSignals(false);
    }
    updateValueFromControls();
}

void OpacityVectorEditDialog::handleIntensitySpin(double value)
{
    const float position = sliderPositionFromIntensity(static_cast<float>(value));
    if (_intensitySlider) {
        _intensitySlider->blockSignals(true);
        _intensitySlider->setValue(static_cast<int>(position * 10.0f));
        _intensitySlider->blockSignals(false);
    }
    updateValueFromControls();
}

void OpacityVectorEditDialog::handleAngleChanged()
{
    updateValueFromControls();
}

void OpacityVectorEditDialog::syncFromValue()
{
    float y_deg = 0.0f;
    float z_deg = 0.0f;
    QuaternionToAngles(_value.angle, y_deg, z_deg);

    if (_angleYSpin) {
        _angleYSpin->setValue(static_cast<int>(std::clamp(y_deg, 0.0f, 179.0f)));
    }
    if (_angleZSpin) {
        _angleZSpin->setValue(static_cast<int>(std::clamp(z_deg, 0.0f, 179.0f)));
    }

    if (_intensitySpin) {
        _intensitySpin->setValue(_value.intensity);
    }

    if (_intensitySlider) {
        const float position = sliderPositionFromIntensity(_value.intensity);
        _intensitySlider->setValue(static_cast<int>(position * 10.0f));
    }
}

float OpacityVectorEditDialog::sliderPositionFromIntensity(float intensity) const
{
    const float percent = std::clamp(intensity / 10.0f, 0.0f, 1.0f);
    const float pos = std::atan(percent * 11.0f) / (84.5f * kDegToRad) * 10.0f;
    return std::clamp(pos, 0.0f, 10.0f);
}

float OpacityVectorEditDialog::intensityFromSliderPosition(float position) const
{
    const float percent = std::tan((position / 10.0f) * 84.5f * kDegToRad) / 11.0f;
    return 10.0f * std::clamp(percent, 0.0f, 1.0f);
}

void OpacityVectorEditDialog::updateValueFromControls()
{
    const float intensity = _intensitySpin ? static_cast<float>(_intensitySpin->value()) : _value.intensity;
    const float y_deg = _angleYSpin ? static_cast<float>(_angleYSpin->value()) : 0.0f;
    const float z_deg = _angleZSpin ? static_cast<float>(_angleZSpin->value()) : 0.0f;

    Matrix3 rot_mat(true);
    rot_mat.Rotate_Y(y_deg * kDegToRad);
    rot_mat.Rotate_Z(z_deg * kDegToRad);

    _value.angle = Build_Quaternion(rot_mat);
    _value.intensity = intensity;
}
