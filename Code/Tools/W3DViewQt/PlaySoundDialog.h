#pragma once

#include <QDialog>
#include <QString>

class AudibleSoundClass;

class PlaySoundDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit PlaySoundDialog(const QString &filename, QWidget *parent = nullptr);
    ~PlaySoundDialog() override;

private slots:
    void playSound();
    void stopSound();

private:
    bool createSound();

    QString _filename;
    AudibleSoundClass *_sound = nullptr;
};
