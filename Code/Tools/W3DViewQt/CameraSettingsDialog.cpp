#include "CameraSettingsDialog.h"

#include "W3DViewport.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kLensConstant = 18.0 / 1000.0;
} // namespace

CameraSettingsDialog::CameraSettingsDialog(W3DViewport *viewport, QWidget *parent)
    : QDialog(parent)
    , _viewport(viewport)
{
    setWindowTitle("Camera Settings");

    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(
        "Use the controls below to specify the camera's clip planes and aspect ratio.", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    _clipCheck = new QCheckBox("&Clip Planes", this);
    connect(_clipCheck, &QCheckBox::toggled, this, &CameraSettingsDialog::onClipCheckChanged);
    layout->addWidget(_clipCheck);

    auto *clip_form = new QFormLayout();
    _nearClipSpin = new QDoubleSpinBox(this);
    _nearClipSpin->setDecimals(2);
    _nearClipSpin->setRange(0.0, 999999.0);
    _nearClipSpin->setSingleStep(0.1);
    clip_form->addRow("&Near:", _nearClipSpin);

    _farClipSpin = new QDoubleSpinBox(this);
    _farClipSpin->setDecimals(2);
    _farClipSpin->setRange(1.0, 999999.0);
    _farClipSpin->setSingleStep(1.0);
    clip_form->addRow("&Far:", _farClipSpin);
    layout->addLayout(clip_form);

    _fovCheck = new QCheckBox("Field of &View", this);
    connect(_fovCheck, &QCheckBox::toggled, this, &CameraSettingsDialog::onFovCheckChanged);
    layout->addWidget(_fovCheck);

    auto *fov_form = new QFormLayout();
    _lensSpin = new QDoubleSpinBox(this);
    _lensSpin->setDecimals(2);
    _lensSpin->setRange(1.0, 200.0);
    _lensSpin->setSuffix(" mm");
    _lensSpin->setSingleStep(1.0);
    connect(_lensSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &CameraSettingsDialog::onLensChanged);
    fov_form->addRow("&Camera Lens:", _lensSpin);

    _hfovSpin = new QDoubleSpinBox(this);
    _hfovSpin->setDecimals(2);
    _hfovSpin->setRange(0.0, 180.0);
    _hfovSpin->setSuffix(" deg");
    _hfovSpin->setSingleStep(1.0);
    connect(_hfovSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &CameraSettingsDialog::onHfovChanged);
    fov_form->addRow("&Horizontal:", _hfovSpin);

    _vfovSpin = new QDoubleSpinBox(this);
    _vfovSpin->setDecimals(2);
    _vfovSpin->setRange(0.0, 180.0);
    _vfovSpin->setSuffix(" deg");
    _vfovSpin->setSingleStep(1.0);
    fov_form->addRow("V&ertical:", _vfovSpin);

    layout->addLayout(fov_form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *reset_button = buttons->addButton("Reset", QDialogButtonBox::ResetRole);
    connect(reset_button, &QPushButton::clicked, this, &CameraSettingsDialog::onReset);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshFromViewport();
}

bool CameraSettingsDialog::isManualFovEnabled() const
{
    return _fovCheck && _fovCheck->isChecked();
}

bool CameraSettingsDialog::isManualClipPlanesEnabled() const
{
    return _clipCheck && _clipCheck->isChecked();
}

double CameraSettingsDialog::hfovDegrees() const
{
    return _hfovSpin ? _hfovSpin->value() : 0.0;
}

double CameraSettingsDialog::vfovDegrees() const
{
    return _vfovSpin ? _vfovSpin->value() : 0.0;
}

double CameraSettingsDialog::lensMm() const
{
    return _lensSpin ? _lensSpin->value() : 0.0;
}

float CameraSettingsDialog::nearClip() const
{
    return _nearClipSpin ? static_cast<float>(_nearClipSpin->value()) : 0.0f;
}

float CameraSettingsDialog::farClip() const
{
    return _farClipSpin ? static_cast<float>(_farClipSpin->value()) : 0.0f;
}

void CameraSettingsDialog::onFovCheckChanged(bool checked)
{
    setFovControlsEnabled(checked);
}

void CameraSettingsDialog::onClipCheckChanged(bool checked)
{
    setClipControlsEnabled(checked);
}

void CameraSettingsDialog::onReset()
{
    if (_viewport) {
        _viewport->setManualFovEnabled(false);
        _viewport->setManualClipPlanesEnabled(false);
        _viewport->resetFov();
        _viewport->resetCamera();
    }

    refreshFromViewport();
}

void CameraSettingsDialog::onHfovChanged(double value)
{
    Q_UNUSED(value);
    updateLensFromHfov();
}

void CameraSettingsDialog::onLensChanged(double value)
{
    Q_UNUSED(value);
    updateFovFromLens();
}

void CameraSettingsDialog::refreshFromViewport()
{
    if (!_viewport) {
        return;
    }

    const bool manual_fov = _viewport->isManualFovEnabled();
    const bool manual_clip = _viewport->isManualClipPlanesEnabled();
    if (_fovCheck) {
        _fovCheck->setChecked(manual_fov);
    }
    if (_clipCheck) {
        _clipCheck->setChecked(manual_clip);
    }

    double hfov_deg = 0.0;
    double vfov_deg = 0.0;
    _viewport->cameraFovDegrees(hfov_deg, vfov_deg);
    if (_hfovSpin) {
        _hfovSpin->setValue(hfov_deg);
    }
    if (_vfovSpin) {
        _vfovSpin->setValue(vfov_deg);
    }

    updateLensFromHfov();

    float znear = 0.0f;
    float zfar = 0.0f;
    _viewport->cameraClipPlanes(znear, zfar);
    if (_nearClipSpin) {
        _nearClipSpin->setValue(znear);
    }
    if (_farClipSpin) {
        _farClipSpin->setValue(zfar);
    }

    setFovControlsEnabled(manual_fov);
    setClipControlsEnabled(manual_clip);
}

void CameraSettingsDialog::updateLensFromHfov()
{
    if (_updating || !_hfovSpin || !_lensSpin) {
        return;
    }

    _updating = true;
    const double hfov_rad = _hfovSpin->value() * kDegToRad;
    if (hfov_rad > 0.0) {
        const double lens = (kLensConstant / std::tan(hfov_rad / 2.0)) * 1000.0;
        _lensSpin->setValue(lens);
    }
    _updating = false;
}

void CameraSettingsDialog::updateFovFromLens()
{
    if (_updating || !_lensSpin || !_hfovSpin || !_vfovSpin) {
        return;
    }

    _updating = true;
    const double lens = _lensSpin->value() / 1000.0;
    if (lens > 0.0) {
        const double hfov = std::atan(kLensConstant / lens) * 2.0;
        const double vfov = (3.0 * hfov) / 4.0;
        _hfovSpin->setValue(hfov * kRadToDeg);
        _vfovSpin->setValue(vfov * kRadToDeg);
    }
    _updating = false;
}

void CameraSettingsDialog::setFovControlsEnabled(bool enabled)
{
    if (_hfovSpin) {
        _hfovSpin->setEnabled(enabled);
    }
    if (_vfovSpin) {
        _vfovSpin->setEnabled(enabled);
    }
    if (_lensSpin) {
        _lensSpin->setEnabled(enabled);
    }
}

void CameraSettingsDialog::setClipControlsEnabled(bool enabled)
{
    if (_nearClipSpin) {
        _nearClipSpin->setEnabled(enabled);
    }
    if (_farClipSpin) {
        _farClipSpin->setEnabled(enabled);
    }
}
