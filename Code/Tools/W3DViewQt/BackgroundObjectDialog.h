#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QListWidget;

class BackgroundObjectDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit BackgroundObjectDialog(const QString &currentName, QWidget *parent = nullptr);

    QString selectedName() const;

private slots:
    void onSelectionChanged();
    void onClear();

private:
    QListWidget *_list = nullptr;
    QLabel *_currentLabel = nullptr;
};
