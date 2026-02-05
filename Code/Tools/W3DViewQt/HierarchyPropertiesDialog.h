#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

class HierarchyPropertiesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit HierarchyPropertiesDialog(const QString &hierarchyName, QWidget *parent = nullptr);

private slots:
    void showSubObjectProperties(QTreeWidgetItem *item, int column);

private:
    void setErrorState(const QString &message);

    QString _hierarchyName;
    QLabel *_description = nullptr;
    QLabel *_polygonCount = nullptr;
    QLabel *_subObjectCount = nullptr;
    QTreeWidget *_subObjectList = nullptr;
};
