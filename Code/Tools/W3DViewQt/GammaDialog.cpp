#include "GammaDialog.h"

#include "dx8wrapper.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

GammaDialog::GammaDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Gamma");

    auto *layout = new QVBoxLayout(this);

    auto *instructions = new QLabel(this);
    instructions->setWordWrap(true);
    instructions->setText(
        "Calibration instructions\n"
        "A. Set Gamma to 1.0 and Monitor Contrast and Brightness to maximum\n"
        "B. Adjust Monitor Brightness down so Bar 3 is barely visible\n"
        "C. Adjust Monitor Contrast as preferred but Bars 1,2,3,4 must be distinguishable from each other\n"
        "D. Set the Gamma using the Slider below so the gray box on the left matches its checkered surroundings\n"
        "E. Press OK to save settings");
    layout->addWidget(instructions);

    _gammaValue = new QLabel(this);
    layout->addWidget(_gammaValue);

    _gammaSlider = new QSlider(Qt::Horizontal, this);
    _gammaSlider->setRange(10, 30);
    connect(_gammaSlider, &QSlider::valueChanged, this, &GammaDialog::onGammaChanged);
    layout->addWidget(_gammaSlider);

    QSettings settings;
    int gamma = settings.value("Config/Gamma", 10).toInt();
    if (gamma < 10) {
        gamma = 10;
    }
    if (gamma > 30) {
        gamma = 30;
    }
    _gammaSlider->setValue(gamma);
    _currentGamma = gamma;
    onGammaChanged(gamma);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        QSettings settings;
        settings.setValue("Config/Gamma", _currentGamma);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void GammaDialog::onGammaChanged(int value)
{
    _currentGamma = value;
    if (_gammaValue) {
        _gammaValue->setText(QString("Gamma: %1").arg(value / 10.0f, 0, 'f', 2));
    }
    applyGamma(value);
}

void GammaDialog::applyGamma(int value)
{
    DX8Wrapper::Set_Gamma(value / 10.0f, 0.0f, 1.0f);
}
