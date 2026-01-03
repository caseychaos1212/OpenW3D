#pragma once

#include <QDialog>
#include <QString>

class QDoubleSpinBox;

class ScaleDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ScaleDialog(double scale, const QString &prompt, QWidget *parent = nullptr);

    double scale() const;

protected:
    void accept() override;

private:
    QDoubleSpinBox *_scaleSpin = nullptr;
};
