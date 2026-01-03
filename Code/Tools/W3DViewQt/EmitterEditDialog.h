#pragma once

#include "part_ldr.h"

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class EmitterEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit EmitterEditDialog(const ParticleEmitterDefClass &definition, QWidget *parent = nullptr);

    ParticleEmitterDefClass *definition() const;
    QString originalName() const;

protected:
    void accept() override;

private slots:
    void browseTexture();
    void toggleLifetime(bool enabled);
    void toggleMaxParticles(bool enabled);

private:
    void loadFromDefinition();
    int findShaderIndex() const;

    ParticleEmitterDefClass _definition;
    QString _originalName;

    QLineEdit *_nameEdit = nullptr;
    QLineEdit *_textureEdit = nullptr;
    QCheckBox *_useLifetimeCheck = nullptr;
    QDoubleSpinBox *_lifetimeSpin = nullptr;
    QComboBox *_shaderCombo = nullptr;
    QComboBox *_renderModeCombo = nullptr;
    QComboBox *_frameModeCombo = nullptr;

    QDoubleSpinBox *_emissionRateSpin = nullptr;
    QDoubleSpinBox *_burstSizeSpin = nullptr;
    QCheckBox *_limitParticlesCheck = nullptr;
    QDoubleSpinBox *_maxParticlesSpin = nullptr;
    QDoubleSpinBox *_fadeTimeSpin = nullptr;

    QDoubleSpinBox *_velocityXSpin = nullptr;
    QDoubleSpinBox *_velocityYSpin = nullptr;
    QDoubleSpinBox *_velocityZSpin = nullptr;
    QDoubleSpinBox *_accelXSpin = nullptr;
    QDoubleSpinBox *_accelYSpin = nullptr;
    QDoubleSpinBox *_accelZSpin = nullptr;
    QDoubleSpinBox *_outwardVelSpin = nullptr;
    QDoubleSpinBox *_inheritVelSpin = nullptr;
    QDoubleSpinBox *_gravitySpin = nullptr;
    QDoubleSpinBox *_elasticitySpin = nullptr;

    QLineEdit *_userStringEdit = nullptr;
    QDoubleSpinBox *_userTypeSpin = nullptr;
};
