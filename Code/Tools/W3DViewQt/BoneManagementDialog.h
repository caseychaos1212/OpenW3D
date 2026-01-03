#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QGroupBox;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class RenderObjClass;
class W3DViewport;

class BoneManagementDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit BoneManagementDialog(RenderObjClass *baseModel,
                                  W3DViewport *viewport,
                                  QWidget *parent = nullptr);
    ~BoneManagementDialog() override;

protected:
    void accept() override;
    void reject() override;

private slots:
    void onBoneSelectionChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onObjectSelectionChanged(int index);
    void onAttachClicked();

private:
    void populateBones();
    void populateObjectList();
    void fillBoneItem(QTreeWidgetItem *boneItem, int boneIndex);
    void updateControls(QTreeWidgetItem *selectedItem);
    void updateAttachButton();
    bool isRenderObjAlreadyAttached(const QString &name) const;
    QTreeWidgetItem *currentBoneItem() const;
    void removeObjectFromBone(QTreeWidgetItem *boneItem, const QString &name);

    RenderObjClass *_baseModel = nullptr;
    RenderObjClass *_backupModel = nullptr;
    W3DViewport *_viewport = nullptr;

    QGroupBox *_boneGroup = nullptr;
    QTreeWidget *_boneTree = nullptr;
    QComboBox *_objectCombo = nullptr;
    QPushButton *_attachButton = nullptr;
    QString _boneName;
    bool _attachMode = true;
};
