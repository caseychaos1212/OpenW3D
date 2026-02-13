#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QRadioButton;

class MeshPropertiesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MeshPropertiesDialog(const QString &meshName, QWidget *parent = nullptr);

private:
    void setErrorState(const QString &message);

    QLabel *_description = nullptr;
    QLabel *_polygonCount = nullptr;
    QLabel *_vertexCount = nullptr;
    QLabel *_userText = nullptr;
    QRadioButton *_meshTypeCollision = nullptr;
    QRadioButton *_meshTypeSkin = nullptr;
    QRadioButton *_meshTypeShadow = nullptr;
    QRadioButton *_meshTypeNormal = nullptr;
    QCheckBox *_collisionPhysical = nullptr;
    QCheckBox *_collisionProjectile = nullptr;
    QCheckBox *_hiddenCheck = nullptr;
};
