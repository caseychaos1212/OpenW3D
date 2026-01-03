#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

class AggregateNameDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AggregateNameDialog(const QString &title,
                                 const QString &defaultName = QString(),
                                 QWidget *parent = nullptr);

    QString name() const;

private:
    QLineEdit *_nameEdit = nullptr;
};
