#pragma once

#include <QDialog>

class QLabel;
class QSlider;

class GammaDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GammaDialog(QWidget *parent = nullptr);

private slots:
    void onGammaChanged(int value);

private:
    void applyGamma(int value);

    QSlider *_gammaSlider = nullptr;
    QLabel *_gammaValue = nullptr;
    int _currentGamma = 10;
};
