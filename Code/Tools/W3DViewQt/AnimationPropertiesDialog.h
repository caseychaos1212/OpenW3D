#pragma once

#include <QDialog>
#include <QString>

class QLabel;

class AnimationPropertiesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AnimationPropertiesDialog(const QString &animationName, QWidget *parent = nullptr);

private:
    void setErrorState(const QString &message);

    QLabel *_description = nullptr;
    QLabel *_frameCount = nullptr;
    QLabel *_frameRate = nullptr;
    QLabel *_totalTime = nullptr;
    QLabel *_hierarchyName = nullptr;
};
