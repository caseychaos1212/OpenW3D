#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class RingRenderObjClass;

class RingEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RingEditDialog(RingRenderObjClass *ring, QWidget *parent = nullptr);
    ~RingEditDialog() override;

    RingRenderObjClass *ring() const;
    QString oldName() const;

protected:
    void accept() override;

private slots:
    void browseTexture();

private:
    void loadFromRing();
    int findShaderIndex() const;

    RingRenderObjClass *_ring = nullptr;
    QString _oldName;

    QLineEdit *_nameEdit = nullptr;
    QLineEdit *_textureEdit = nullptr;
    QDoubleSpinBox *_lifetimeSpin = nullptr;
    QComboBox *_shaderCombo = nullptr;
    QCheckBox *_cameraAlignCheck = nullptr;
    QCheckBox *_loopCheck = nullptr;
    QSpinBox *_tilingSpin = nullptr;

    QTableWidget *_colorKeysTable = nullptr;
    QTableWidget *_alphaKeysTable = nullptr;

    QDoubleSpinBox *_innerXSpin = nullptr;
    QDoubleSpinBox *_innerYSpin = nullptr;
    QDoubleSpinBox *_outerXSpin = nullptr;
    QDoubleSpinBox *_outerYSpin = nullptr;
    QTableWidget *_innerScaleTable = nullptr;
    QTableWidget *_outerScaleTable = nullptr;
};
