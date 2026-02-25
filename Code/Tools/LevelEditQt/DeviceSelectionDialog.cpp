#include "DeviceSelectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace leveledit_qt {

DeviceSelectionDialog::DeviceSelectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Render Device Selection"));

    _deviceCombo = new QComboBox(this);
    _deviceCombo->addItem(QStringLiteral("Primary Display Adapter (stub)"), 0);

    _bppCombo = new QComboBox(this);
    _bppCombo->addItem(QStringLiteral("32-bit"), 32);
    _bppCombo->addItem(QStringLiteral("16-bit"), 16);

    _windowedCheck = new QCheckBox(QStringLiteral("Windowed"), this);
    _windowedCheck->setChecked(true);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Device:"), _deviceCombo);
    form->addRow(QStringLiteral("Color depth:"), _bppCombo);
    form->addRow(QString(), _windowedCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

int DeviceSelectionDialog::deviceIndex() const
{
    return _deviceCombo->currentData().toInt();
}

int DeviceSelectionDialog::bitsPerPixel() const
{
    return _bppCombo->currentData().toInt();
}

bool DeviceSelectionDialog::isWindowed() const
{
    return _windowedCheck->isChecked();
}

} // namespace leveledit_qt
