#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTableWidget;
class SphereRenderObjClass;

class SphereEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SphereEditDialog(SphereRenderObjClass *sphere, QWidget *parent = nullptr);
    ~SphereEditDialog() override;

    SphereRenderObjClass *sphere() const;
    QString oldName() const;

protected:
    void accept() override;

private slots:
    void browseTexture();

private:
    void loadFromSphere();
    int findShaderIndex() const;

    SphereRenderObjClass *_sphere = nullptr;
    QString _oldName;

    QLineEdit *_nameEdit = nullptr;
    QLineEdit *_textureEdit = nullptr;
    QDoubleSpinBox *_lifetimeSpin = nullptr;
    QComboBox *_shaderCombo = nullptr;
    QCheckBox *_cameraAlignCheck = nullptr;
    QCheckBox *_loopCheck = nullptr;

    QTableWidget *_colorKeysTable = nullptr;
    QTableWidget *_alphaKeysTable = nullptr;
    QTableWidget *_vectorKeysTable = nullptr;
    QCheckBox *_useVectorCheck = nullptr;
    QCheckBox *_invertVectorCheck = nullptr;

    QDoubleSpinBox *_sizeXSpin = nullptr;
    QDoubleSpinBox *_sizeYSpin = nullptr;
    QDoubleSpinBox *_sizeZSpin = nullptr;
    QTableWidget *_scaleKeysTable = nullptr;
};
