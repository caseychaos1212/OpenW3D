#include "CameraDistanceDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

CameraDistanceDialog::CameraDistanceDialog(float distance, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Camera Distance");

    auto *layout = new QFormLayout(this);

    _distanceSpin = new QDoubleSpinBox(this);
    _distanceSpin->setDecimals(2);
    _distanceSpin->setRange(0.0, 25000.0);
    _distanceSpin->setSingleStep(10.0);
    _distanceSpin->setValue(distance);
    layout->addRow("&Camera Distance:", _distanceSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

float CameraDistanceDialog::distance() const
{
    return _distanceSpin ? static_cast<float>(_distanceSpin->value()) : 0.0f;
}
