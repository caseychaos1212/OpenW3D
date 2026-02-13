#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

class AnimatedSoundOptionsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AnimatedSoundOptionsDialog(const QString &definitionLibraryPath,
                                        const QString &iniPath,
                                        const QString &dataPath,
                                        QWidget *parent = nullptr);

    QString definitionLibraryPath() const;
    QString iniPath() const;
    QString dataPath() const;

    static void LoadAnimatedSoundSettings();

private slots:
    void browseDefinitionLibrary();
    void browseIniPath();
    void browseDataPath();

private:
    QLineEdit *_definitionLibraryEdit = nullptr;
    QLineEdit *_iniEdit = nullptr;
    QLineEdit *_dataPathEdit = nullptr;
};
