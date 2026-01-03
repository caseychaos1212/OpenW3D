#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QRadioButton;
class QSlider;
class SoundRenderObjClass;

class SoundEditDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SoundEditDialog(SoundRenderObjClass *sound, QWidget *parent = nullptr);
    ~SoundEditDialog() override;

    SoundRenderObjClass *sound() const;
    QString oldName() const;

protected:
    void accept() override;

private slots:
    void browseSoundFile();
    void toggleSoundType();
    void playSound();

private:
    void loadFromSound();
    void updateEnableState();

    SoundRenderObjClass *_sound = nullptr;
    QString _oldName;

    QLineEdit *_nameEdit = nullptr;
    QLineEdit *_fileEdit = nullptr;
    QRadioButton *_radio2d = nullptr;
    QRadioButton *_radio3d = nullptr;
    QRadioButton *_radioMusic = nullptr;
    QRadioButton *_radioEffect = nullptr;
    QCheckBox *_infiniteLoops = nullptr;
    QCheckBox *_stopWhenHidden = nullptr;
    QSlider *_volumeSlider = nullptr;
    QSlider *_prioritySlider = nullptr;
    QDoubleSpinBox *_dropOffEdit = nullptr;
    QDoubleSpinBox *_maxVolEdit = nullptr;
    QDoubleSpinBox *_triggerRadiusEdit = nullptr;
};
