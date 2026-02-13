#pragma once

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class W3DViewport;

class CameraSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingsDialog(W3DViewport *viewport, QWidget *parent = nullptr);

    bool isManualFovEnabled() const;
    bool isManualClipPlanesEnabled() const;
    double hfovDegrees() const;
    double vfovDegrees() const;
    double lensMm() const;
    float nearClip() const;
    float farClip() const;

private slots:
    void onFovCheckChanged(bool checked);
    void onClipCheckChanged(bool checked);
    void onReset();
    void onHfovChanged(double value);
    void onLensChanged(double value);

private:
    void refreshFromViewport();
    void updateLensFromHfov();
    void updateFovFromLens();
    void setFovControlsEnabled(bool enabled);
    void setClipControlsEnabled(bool enabled);

    W3DViewport *_viewport = nullptr;
    QCheckBox *_fovCheck = nullptr;
    QCheckBox *_clipCheck = nullptr;
    QDoubleSpinBox *_hfovSpin = nullptr;
    QDoubleSpinBox *_vfovSpin = nullptr;
    QDoubleSpinBox *_lensSpin = nullptr;
    QDoubleSpinBox *_nearClipSpin = nullptr;
    QDoubleSpinBox *_farClipSpin = nullptr;
    bool _updating = false;
};
