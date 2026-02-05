#include "AggregateNameDialog.h"

#include "w3d_file.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

AggregateNameDialog::AggregateNameDialog(const QString &title,
                                         const QString &defaultName,
                                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    _nameEdit = new QLineEdit(this);
    _nameEdit->setMaxLength(W3D_NAME_LEN - 1);
    _nameEdit->setText(defaultName);
    form->addRow("Name:", _nameEdit);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString AggregateNameDialog::name() const
{
    return _nameEdit ? _nameEdit->text() : QString();
}
