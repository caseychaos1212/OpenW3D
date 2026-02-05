#include "TexturePathDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

TexturePathDialog::TexturePathDialog(const QString &path1, const QString &path2, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Texture Paths");

    auto *layout = new QFormLayout(this);

    _path1Edit = new QLineEdit(QDir::toNativeSeparators(path1), this);
    auto *browse1 = new QPushButton("Browse...", this);
    connect(browse1, &QPushButton::clicked, this, &TexturePathDialog::browsePath1);

    auto *row1 = new QHBoxLayout();
    row1->addWidget(_path1Edit);
    row1->addWidget(browse1);
    layout->addRow("Path &1:", row1);

    _path2Edit = new QLineEdit(QDir::toNativeSeparators(path2), this);
    auto *browse2 = new QPushButton("Browse...", this);
    connect(browse2, &QPushButton::clicked, this, &TexturePathDialog::browsePath2);

    auto *row2 = new QHBoxLayout();
    row2->addWidget(_path2Edit);
    row2->addWidget(browse2);
    layout->addRow("Path &2:", row2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

QString TexturePathDialog::path1() const
{
    return _path1Edit ? _path1Edit->text().trimmed() : QString();
}

QString TexturePathDialog::path2() const
{
    return _path2Edit ? _path2Edit->text().trimmed() : QString();
}

void TexturePathDialog::browsePath1()
{
    const QString start = _path1Edit ? _path1Edit->text() : QString();
    const QString dir = QFileDialog::getExistingDirectory(this, "Texture Path 1", start);
    if (!dir.isEmpty() && _path1Edit) {
        _path1Edit->setText(QDir::toNativeSeparators(dir));
    }
}

void TexturePathDialog::browsePath2()
{
    const QString start = _path2Edit ? _path2Edit->text() : QString();
    const QString dir = QFileDialog::getExistingDirectory(this, "Texture Path 2", start);
    if (!dir.isEmpty() && _path2Edit) {
        _path2Edit->setText(QDir::toNativeSeparators(dir));
    }
}
