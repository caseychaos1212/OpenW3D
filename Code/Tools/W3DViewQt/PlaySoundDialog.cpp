#include "PlaySoundDialog.h"

#include "AudibleSound.h"
#include "WWAudio.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

PlaySoundDialog::PlaySoundDialog(const QString &filename, QWidget *parent)
    : QDialog(parent),
      _filename(filename)
{
    setWindowTitle("Play Sound");

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(this);
    label->setText(QString("Sound file: %1").arg(_filename));
    layout->addWidget(label);

    auto *button_row = new QHBoxLayout();
    auto *play_button = new QPushButton("Play", this);
    auto *stop_button = new QPushButton("Stop", this);
    button_row->addWidget(play_button);
    button_row->addWidget(stop_button);
    layout->addLayout(button_row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(play_button, &QPushButton::clicked, this, &PlaySoundDialog::playSound);
    connect(stop_button, &QPushButton::clicked, this, &PlaySoundDialog::stopSound);

    if (!createSound()) {
        reject();
    }
}

PlaySoundDialog::~PlaySoundDialog()
{
    stopSound();
    if (_sound) {
        _sound->Release_Ref();
        _sound = nullptr;
    }
}

bool PlaySoundDialog::createSound()
{
    if (_filename.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Play Sound", "No sound file specified.");
        return false;
    }

    auto *audio = WWAudioClass::Get_Instance();
    if (!audio) {
        QMessageBox::warning(this, "Play Sound", "Audio system is not available.");
        return false;
    }

    const QString file_name = QFileInfo(_filename).fileName();
    if (file_name.isEmpty()) {
        QMessageBox::warning(this, "Play Sound", "Invalid sound filename.");
        return false;
    }

    const QByteArray name_bytes = file_name.toLatin1();
    _sound = audio->Create_Sound_Effect(name_bytes.constData());
    if (!_sound) {
        QMessageBox::warning(this, "Play Sound", QString("Cannot find sound file: %1").arg(file_name));
        return false;
    }

    playSound();
    return true;
}

void PlaySoundDialog::playSound()
{
    if (_sound) {
        _sound->Stop();
        _sound->Play();
    }
}

void PlaySoundDialog::stopSound()
{
    if (_sound) {
        _sound->Stop();
    }
}
