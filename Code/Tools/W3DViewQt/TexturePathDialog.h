#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

class TexturePathDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit TexturePathDialog(const QString &path1, const QString &path2, QWidget *parent = nullptr);

    QString path1() const;
    QString path2() const;

private slots:
    void browsePath1();
    void browsePath2();

private:
    QLineEdit *_path1Edit = nullptr;
    QLineEdit *_path2Edit = nullptr;
};
