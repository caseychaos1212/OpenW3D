#include "ScaleDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

ScaleDialog::ScaleDialog(double scale, const QString &prompt, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Scale");

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(prompt, this);
    label->setWordWrap(true);
    layout->addWidget(label);

    _scaleSpin = new QDoubleSpinBox(this);
    _scaleSpin->setRange(0.01, 100.0);
    _scaleSpin->setDecimals(2);
    _scaleSpin->setValue(scale);
    layout->addWidget(_scaleSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ScaleDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

double ScaleDialog::scale() const
{
    return _scaleSpin ? _scaleSpin->value() : 1.0;
}

void ScaleDialog::accept()
{
    if (!_scaleSpin || _scaleSpin->value() <= 0.0) {
        QMessageBox::information(this, "Invalid Scale", "Scale must be a value greater than zero.");
        return;
    }

    QDialog::accept();
}
