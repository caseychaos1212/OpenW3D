#include "BackgroundObjectDialog.h"

#include "assetmgr.h"
#include "rendobj.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

BackgroundObjectDialog::BackgroundObjectDialog(const QString &currentName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Background Object");

    auto *layout = new QVBoxLayout(this);

    _currentLabel = new QLabel(this);
    layout->addWidget(_currentLabel);

    _list = new QListWidget(this);
    _list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(_list);

    connect(_list, &QListWidget::itemSelectionChanged, this,
            &BackgroundObjectDialog::onSelectionChanged);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *clear_button = buttons->addButton("Clear", QDialogButtonBox::ResetRole);
    connect(clear_button, &QPushButton::clicked, this, &BackgroundObjectDialog::onClear);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return;
    }

    RenderObjIterator *iterator = asset_manager->Create_Render_Obj_Iterator();
    if (!iterator) {
        return;
    }

    for (iterator->First(); !iterator->Is_Done(); iterator->Next()) {
        const char *name = iterator->Current_Item_Name();
        if (!name || !name[0]) {
            continue;
        }

        if (!asset_manager->Render_Obj_Exists(name)) {
            continue;
        }

        if (iterator->Current_Item_Class_ID() != RenderObjClass::CLASSID_HMODEL) {
            continue;
        }

        _list->addItem(QString::fromLatin1(name));
    }

    asset_manager->Release_Render_Obj_Iterator(iterator);

    if (!currentName.isEmpty()) {
        const QList<QListWidgetItem *> matches = _list->findItems(currentName, Qt::MatchFixedString);
        if (!matches.isEmpty()) {
            _list->setCurrentItem(matches.front());
        }
    }

    if (!_list->currentItem() && _list->count() > 0) {
        _list->setCurrentRow(0);
    }

    onSelectionChanged();
}

QString BackgroundObjectDialog::selectedName() const
{
    auto *item = _list ? _list->currentItem() : nullptr;
    return item ? item->text() : QString();
}

void BackgroundObjectDialog::onSelectionChanged()
{
    const QString name = selectedName();
    if (_currentLabel) {
        _currentLabel->setText(name.isEmpty() ? "Current Object: (none)"
                                             : QString("Current Object: %1").arg(name));
    }
}

void BackgroundObjectDialog::onClear()
{
    if (_list) {
        _list->clearSelection();
    }
    onSelectionChanged();
}
