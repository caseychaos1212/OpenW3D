#include "HierarchyPropertiesDialog.h"

#include "MeshPropertiesDialog.h"

#include "assetmgr.h"
#include "rendobj.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
QLabel *MakeValueLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setText("n/a");
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
}

HierarchyPropertiesDialog::HierarchyPropertiesDialog(const QString &hierarchyName, QWidget *parent)
    : QDialog(parent)
    , _hierarchyName(hierarchyName)
{
    setWindowTitle("Hierarchy Properties");

    auto *layout = new QVBoxLayout(this);

    _description = new QLabel(this);
    _description->setWordWrap(true);
    _description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_description);

    auto *form = new QFormLayout();
    _polygonCount = MakeValueLabel(this);
    _subObjectCount = MakeValueLabel(this);
    form->addRow("Total Polygons:", _polygonCount);
    form->addRow("Subobjects:", _subObjectCount);
    layout->addLayout(form);

    _subObjectList = new QTreeWidget(this);
    _subObjectList->setHeaderLabels(QStringList() << "Name");
    _subObjectList->setRootIsDecorated(false);
    _subObjectList->setItemsExpandable(false);
    _subObjectList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(_subObjectList,
            &QTreeWidget::itemDoubleClicked,
            this,
            &HierarchyPropertiesDialog::showSubObjectProperties);
    layout->addWidget(_subObjectList);

    auto *buttons_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons_box);

    if (_hierarchyName.isEmpty()) {
        setErrorState("No hierarchy selected.");
        return;
    }

    _description->setText(QString("Hierarchy: %1").arg(_hierarchyName));

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        setErrorState("WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = _hierarchyName.toLatin1();
    RenderObjClass *hierarchy = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!hierarchy) {
        setErrorState("Failed to load hierarchy.");
        return;
    }

    _polygonCount->setText(QString::number(hierarchy->Get_Num_Polys()));

    const int sub_count = hierarchy->Get_Num_Sub_Objects();
    _subObjectCount->setText(QString::number(sub_count));

    for (int index = 0; index < sub_count; ++index) {
        RenderObjClass *sub_obj = hierarchy->Get_Sub_Object(index);
        if (!sub_obj) {
            continue;
        }

        const char *sub_name = sub_obj->Get_Name();
        if (sub_name && sub_name[0]) {
            auto *item = new QTreeWidgetItem(_subObjectList);
            item->setText(0, QString::fromLatin1(sub_name));
        }

        sub_obj->Release_Ref();
    }

    _subObjectList->resizeColumnToContents(0);
    hierarchy->Release_Ref();
}

void HierarchyPropertiesDialog::showSubObjectProperties(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0) {
        return;
    }

    const QString name = item->text(0);
    if (name.isEmpty()) {
        return;
    }

    MeshPropertiesDialog dialog(name, this);
    dialog.exec();
}

void HierarchyPropertiesDialog::setErrorState(const QString &message)
{
    if (_description) {
        _description->setText(message);
    }
    if (_polygonCount) {
        _polygonCount->setText("n/a");
    }
    if (_subObjectCount) {
        _subObjectCount->setText("n/a");
    }
    if (_subObjectList) {
        _subObjectList->setEnabled(false);
    }
}
