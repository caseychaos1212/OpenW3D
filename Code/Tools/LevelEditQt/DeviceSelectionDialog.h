#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;

namespace leveledit_qt {

class DeviceSelectionDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceSelectionDialog(QWidget *parent = nullptr);

    int deviceIndex() const;
    int bitsPerPixel() const;
    bool isWindowed() const;

private:
    QComboBox *_deviceCombo = nullptr;
    QComboBox *_bppCombo = nullptr;
    QCheckBox *_windowedCheck = nullptr;
};

} // namespace leveledit_qt
