#include "AnimatedSoundOptionsDialog.h"

#include "animatedsoundmgr.h"
#include "chunkio.h"
#include "definitionmgr.h"
#include "ffactory.h"
#include "wwdebug.h"
#include "wwfile.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
QString NormalizePath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(path.trimmed());
}

QString StartDirectoryForFile(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QDir::currentPath();
    }

    const QFileInfo info(path);
    if (info.exists()) {
        return info.absolutePath();
    }

    return QFileInfo(path).absolutePath();
}
}

AnimatedSoundOptionsDialog::AnimatedSoundOptionsDialog(const QString &definitionLibraryPath,
                                                       const QString &iniPath,
                                                       const QString &dataPath,
                                                       QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Animated Sound Options");

    auto *layout = new QVBoxLayout(this);

    auto *instructions = new QLabel(
        "Use the controls below to configure the animation-triggered sound effect settings for the viewer.",
        this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);

    auto *form = new QFormLayout();

    _definitionLibraryEdit = new QLineEdit(QDir::toNativeSeparators(definitionLibraryPath), this);
    auto *definitionBrowse = new QPushButton("Browse...", this);
    connect(definitionBrowse, &QPushButton::clicked, this,
            &AnimatedSoundOptionsDialog::browseDefinitionLibrary);

    auto *definitionRow = new QHBoxLayout();
    definitionRow->addWidget(_definitionLibraryEdit);
    definitionRow->addWidget(definitionBrowse);
    form->addRow("Sound Preset Library Path:", definitionRow);

    _iniEdit = new QLineEdit(QDir::toNativeSeparators(iniPath), this);
    auto *iniBrowse = new QPushButton("Browse...", this);
    connect(iniBrowse, &QPushButton::clicked, this, &AnimatedSoundOptionsDialog::browseIniPath);

    auto *iniRow = new QHBoxLayout();
    iniRow->addWidget(_iniEdit);
    iniRow->addWidget(iniBrowse);
    form->addRow("Animated Sound INI Path:", iniRow);

    _dataPathEdit = new QLineEdit(QDir::toNativeSeparators(dataPath), this);
    auto *dataBrowse = new QPushButton("Browse...", this);
    connect(dataBrowse, &QPushButton::clicked, this, &AnimatedSoundOptionsDialog::browseDataPath);

    auto *dataRow = new QHBoxLayout();
    dataRow->addWidget(_dataPathEdit);
    dataRow->addWidget(dataBrowse);
    form->addRow("Sound File(s) Path:", dataRow);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString AnimatedSoundOptionsDialog::definitionLibraryPath() const
{
    return _definitionLibraryEdit ? _definitionLibraryEdit->text().trimmed() : QString();
}

QString AnimatedSoundOptionsDialog::iniPath() const
{
    return _iniEdit ? _iniEdit->text().trimmed() : QString();
}

QString AnimatedSoundOptionsDialog::dataPath() const
{
    return _dataPathEdit ? _dataPathEdit->text().trimmed() : QString();
}

void AnimatedSoundOptionsDialog::LoadAnimatedSoundSettings()
{
    DefinitionMgrClass::Free_Definitions();

    QSettings settings;
    const QString definition_path = NormalizePath(settings.value("Config/SoundDefLibPath").toString());
    const QString ini_path = NormalizePath(settings.value("Config/AnimSoundINIPath").toString());
    const QString data_path = NormalizePath(settings.value("Config/AnimSoundDataPath").toString());

    if (_TheFileFactory && !definition_path.isEmpty()) {
        const QByteArray native = QDir::toNativeSeparators(definition_path).toLocal8Bit();
        FileClass *file = _TheFileFactory->Get_File(native.constData());
        if (file != nullptr) {
            file->Open(FileClass::READ);
            ChunkLoadClass cload(file);
            SaveLoadSystemClass::Load(cload);
            file->Close();
            _TheFileFactory->Return_File(file);
        } else {
            WWDEBUG_SAY(("Failed to load file %s\n", native.constData()));
        }
    }

    AnimatedSoundMgrClass::Shutdown();
    if (ini_path.isEmpty()) {
        AnimatedSoundMgrClass::Initialize("");
    } else {
        const QByteArray native = QDir::toNativeSeparators(ini_path).toLocal8Bit();
        AnimatedSoundMgrClass::Initialize(native.constData());
    }

    if (_TheSimpleFileFactory && !data_path.isEmpty()) {
        const QByteArray native = QDir::toNativeSeparators(data_path).toLocal8Bit();
        _TheSimpleFileFactory->Append_Sub_Directory(native.constData());
    }
}

void AnimatedSoundOptionsDialog::browseDefinitionLibrary()
{
    const QString start = _definitionLibraryEdit ? _definitionLibraryEdit->text() : QString();
    const QString initial_dir = StartDirectoryForFile(start);
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Sound Preset Library",
        initial_dir,
        "Definition Database Files (*.ddb)");
    if (!path.isEmpty() && _definitionLibraryEdit) {
        _definitionLibraryEdit->setText(QDir::toNativeSeparators(path));
    }
}

void AnimatedSoundOptionsDialog::browseIniPath()
{
    const QString start = _iniEdit ? _iniEdit->text() : QString();
    const QString initial_dir = StartDirectoryForFile(start);
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Animated Sound INI",
        initial_dir,
        "INI Files (*.ini)");
    if (!path.isEmpty() && _iniEdit) {
        _iniEdit->setText(QDir::toNativeSeparators(path));
    }
}

void AnimatedSoundOptionsDialog::browseDataPath()
{
    const QString start = _dataPathEdit ? _dataPathEdit->text() : QString();
    const QString dir = QFileDialog::getExistingDirectory(this, "Pick Sound Path", start);
    if (!dir.isEmpty() && _dataPathEdit) {
        _dataPathEdit->setText(QDir::toNativeSeparators(dir));
    }
}
