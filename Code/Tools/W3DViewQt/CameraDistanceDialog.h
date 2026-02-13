#pragma once

#include <QDialog>

class QDoubleSpinBox;

class CameraDistanceDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CameraDistanceDialog(float distance, QWidget *parent = nullptr);

    float distance() const;

private:
    QDoubleSpinBox *_distanceSpin = nullptr;
};
