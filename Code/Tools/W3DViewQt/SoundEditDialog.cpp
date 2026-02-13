#include "SoundEditDialog.h"

#include "PlaySoundDialog.h"

#include "AudibleSound.h"
#include "Sound3D.h"
#include "WWAudio.h"
#include "soundrobj.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {
constexpr float kDefaultDropOff = 100.0f;
constexpr float kDefaultMaxVol = 10.0f;
constexpr float kDefaultPriority = 0.5f;
constexpr float kDefaultVolume = 1.0f;
}

SoundEditDialog::SoundEditDialog(SoundRenderObjClass *sound, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Sound Object");

    if (sound) {
        _sound = sound;
        _sound->Add_Ref();
    } else {
        _sound = new SoundRenderObjClass;
    }

    if (_sound && _sound->Get_Name()) {
        _oldName = QString::fromLatin1(_sound->Get_Name());
    }

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();

    _nameEdit = new QLineEdit(this);
    form->addRow("Name:", _nameEdit);

    auto *file_row = new QHBoxLayout();
    _fileEdit = new QLineEdit(this);
    auto *browse_button = new QPushButton("Browse...", this);
    file_row->addWidget(_fileEdit);
    file_row->addWidget(browse_button);
    form->addRow("Filename:", file_row);
    connect(browse_button, &QPushButton::clicked, this, &SoundEditDialog::browseSoundFile);

    auto *type_group = new QGroupBox("Sound Type", this);
    auto *type_layout = new QHBoxLayout(type_group);
    _radio3d = new QRadioButton("3D", type_group);
    _radio2d = new QRadioButton("2D", type_group);
    type_layout->addWidget(_radio3d);
    type_layout->addWidget(_radio2d);
    form->addRow(type_group);

    auto *category_group = new QGroupBox("Category", this);
    auto *category_layout = new QHBoxLayout(category_group);
    _radioEffect = new QRadioButton("Sound Effect", category_group);
    _radioMusic = new QRadioButton("Music", category_group);
    category_layout->addWidget(_radioEffect);
    category_layout->addWidget(_radioMusic);
    form->addRow(category_group);

    _infiniteLoops = new QCheckBox("Infinite Loops", this);
    form->addRow(_infiniteLoops);

    _stopWhenHidden = new QCheckBox("Stop When Hidden", this);
    form->addRow(_stopWhenHidden);

    _volumeSlider = new QSlider(Qt::Horizontal, this);
    _volumeSlider->setRange(0, 100);
    form->addRow("Volume:", _volumeSlider);

    _prioritySlider = new QSlider(Qt::Horizontal, this);
    _prioritySlider->setRange(0, 100);
    form->addRow("Priority:", _prioritySlider);

    _dropOffEdit = new QDoubleSpinBox(this);
    _dropOffEdit->setRange(0.0, 100000.0);
    _dropOffEdit->setDecimals(2);
    form->addRow("Drop-off Radius:", _dropOffEdit);

    _maxVolEdit = new QDoubleSpinBox(this);
    _maxVolEdit->setRange(0.0, 100000.0);
    _maxVolEdit->setDecimals(2);
    form->addRow("Max-Vol Radius:", _maxVolEdit);

    _triggerRadiusEdit = new QDoubleSpinBox(this);
    _triggerRadiusEdit->setRange(0.0, 100000.0);
    _triggerRadiusEdit->setDecimals(2);
    form->addRow("Trigger Radius:", _triggerRadiusEdit);

    layout->addLayout(form);

    auto *play_button = new QPushButton("Play...", this);
    connect(play_button, &QPushButton::clicked, this, &SoundEditDialog::playSound);
    layout->addWidget(play_button);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SoundEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SoundEditDialog::reject);
    layout->addWidget(buttons);

    connect(_radio2d, &QRadioButton::toggled, this, &SoundEditDialog::toggleSoundType);
    connect(_radio3d, &QRadioButton::toggled, this, &SoundEditDialog::toggleSoundType);

    loadFromSound();
    updateEnableState();
}

SoundEditDialog::~SoundEditDialog()
{
    if (_sound) {
        _sound->Release_Ref();
        _sound = nullptr;
    }
}

SoundRenderObjClass *SoundEditDialog::sound() const
{
    if (_sound) {
        _sound->Add_Ref();
    }
    return _sound;
}

QString SoundEditDialog::oldName() const
{
    return _oldName;
}

void SoundEditDialog::accept()
{
    if (!_sound) {
        QDialog::reject();
        return;
    }

    const QString name = _nameEdit ? _nameEdit->text().trimmed() : QString();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Sound Object", "Invalid object name. Please enter a new name.");
        return;
    }

    auto *audio = WWAudioClass::Get_Instance();
    if (!audio) {
        QMessageBox::warning(this, "Sound Object", "Audio system is not available.");
        return;
    }

    const QString filename = _fileEdit ? _fileEdit->text().trimmed() : QString();
    const QString file_name_only = QFileInfo(filename).fileName();
    if (file_name_only.isEmpty()) {
        QMessageBox::warning(this, "Sound Object", "Invalid sound filename.");
        return;
    }

    AudibleSoundClass *sound = nullptr;
    const bool is_3d = _radio3d && _radio3d->isChecked();
    const QByteArray file_bytes = file_name_only.toLatin1();
    if (is_3d) {
        sound = audio->Create_3D_Sound(file_bytes.constData());
    } else {
        sound = audio->Create_Sound_Effect(file_bytes.constData());
    }

    if (!sound) {
        QMessageBox::warning(this, "Sound Object", "Failed to create sound object.");
        return;
    }

    const float priority = _prioritySlider ? _prioritySlider->value() / 100.0f : kDefaultPriority;
    const float volume = _volumeSlider ? _volumeSlider->value() / 100.0f : kDefaultVolume;
    sound->Set_Priority(priority);
    sound->Set_Volume(volume);

    const int loop_count = _infiniteLoops && _infiniteLoops->isChecked() ? 0 : 1;
    sound->Set_Loop_Count(loop_count);

    const bool is_music = _radioMusic && _radioMusic->isChecked();
    sound->Set_Type(is_music ? AudibleSoundClass::TYPE_MUSIC
                             : AudibleSoundClass::TYPE_SOUND_EFFECT);

    float drop_off = _dropOffEdit ? static_cast<float>(_dropOffEdit->value()) : kDefaultDropOff;
    float max_vol = _maxVolEdit ? static_cast<float>(_maxVolEdit->value()) : kDefaultMaxVol;
    float trigger = _triggerRadiusEdit ? static_cast<float>(_triggerRadiusEdit->value()) : kDefaultDropOff;

    if (is_3d) {
        sound->Set_DropOff_Radius(drop_off);
        auto *sound_3d = sound->As_Sound3DClass();
        if (sound_3d) {
            sound_3d->Set_Max_Vol_Radius(max_vol);
        }
    } else {
        sound->Set_DropOff_Radius(trigger);
    }

    AudibleSoundDefinitionClass definition;
    definition.Initialize_From_Sound(sound);
    sound->Release_Ref();

    _sound->Set_Sound(&definition);

    if (_stopWhenHidden && _stopWhenHidden->isChecked()) {
        _sound->Set_Flags(SoundRenderObjClass::FLAG_STOP_WHEN_HIDDEN);
    } else {
        _sound->Set_Flags(0);
    }

    const QByteArray name_bytes = name.toLatin1();
    _sound->Set_Name(name_bytes.constData());

    QDialog::accept();
}

void SoundEditDialog::browseSoundFile()
{
    const QString start = _fileEdit ? _fileEdit->text() : QString();
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Select Sound File",
        start,
        "All Sound Files (*.wav *.mp3);;WAV Files (*.wav);;MP3 Files (*.mp3)");
    if (!path.isEmpty() && _fileEdit) {
        _fileEdit->setText(QFileInfo(path).fileName());
    }
}

void SoundEditDialog::toggleSoundType()
{
    updateEnableState();
}

void SoundEditDialog::playSound()
{
    const QString filename = _fileEdit ? _fileEdit->text().trimmed() : QString();
    if (filename.isEmpty()) {
        QMessageBox::warning(this, "Play Sound", "No sound file specified.");
        return;
    }

    PlaySoundDialog dialog(filename, this);
    dialog.exec();
}

void SoundEditDialog::loadFromSound()
{
    if (!_sound) {
        return;
    }

    if (_nameEdit) {
        const char *name = _sound->Get_Name();
        if (name) {
            _nameEdit->setText(QString::fromLatin1(name));
        }
    }

    bool stop_on_hide = _sound->Get_Flag(SoundRenderObjClass::FLAG_STOP_WHEN_HIDDEN);
    float drop_off_radius = kDefaultDropOff;
    float max_vol_radius = kDefaultMaxVol;
    float priority = kDefaultPriority;
    bool is_3d = true;
    bool is_music = false;
    int loop_count = 1;
    float volume = kDefaultVolume;
    QString filename;

    AudibleSoundClass *sound = _sound->Peek_Sound();
    if (sound) {
        filename = QString::fromLatin1(sound->Get_Filename());
        drop_off_radius = sound->Get_DropOff_Radius();
        priority = sound->Peek_Priority();
        is_3d = sound->As_Sound3DClass() != nullptr;
        is_music = sound->Get_Type() == AudibleSoundClass::TYPE_MUSIC;
        loop_count = sound->Get_Loop_Count();
        volume = sound->Get_Volume();

        auto *sound_3d = sound->As_Sound3DClass();
        if (sound_3d) {
            max_vol_radius = sound_3d->Get_Max_Vol_Radius();
        }
    }

    if (_fileEdit) {
        _fileEdit->setText(filename);
    }
    if (_infiniteLoops) {
        _infiniteLoops->setChecked(loop_count == 0);
    }
    if (_radio3d) {
        _radio3d->setChecked(is_3d);
    }
    if (_radio2d) {
        _radio2d->setChecked(!is_3d);
    }
    if (_radioMusic) {
        _radioMusic->setChecked(is_music);
    }
    if (_radioEffect) {
        _radioEffect->setChecked(!is_music);
    }
    if (_stopWhenHidden) {
        _stopWhenHidden->setChecked(stop_on_hide);
    }
    if (_volumeSlider) {
        _volumeSlider->setValue(static_cast<int>(volume * 100.0f));
    }
    if (_prioritySlider) {
        _prioritySlider->setValue(static_cast<int>(priority * 100.0f));
    }
    if (_dropOffEdit) {
        _dropOffEdit->setValue(drop_off_radius);
    }
    if (_maxVolEdit) {
        _maxVolEdit->setValue(max_vol_radius);
    }
    if (_triggerRadiusEdit) {
        _triggerRadiusEdit->setValue(drop_off_radius);
    }
}

void SoundEditDialog::updateEnableState()
{
    const bool enable_3d = _radio3d && _radio3d->isChecked();
    if (_maxVolEdit) {
        _maxVolEdit->setEnabled(enable_3d);
    }
    if (_dropOffEdit) {
        _dropOffEdit->setEnabled(enable_3d);
    }
    if (_triggerRadiusEdit) {
        _triggerRadiusEdit->setEnabled(!enable_3d);
    }
}
