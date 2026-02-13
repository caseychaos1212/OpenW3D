#include "AddToLineupDialog.h"

#include "W3DViewport.h"

#include "assetmgr.h"
#include "rendobj.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QVBoxLayout>

AddToLineupDialog::AddToLineupDialog(W3DViewport *viewport, QWidget *parent)
    : QDialog(parent)
    , _viewport(viewport)
{
    setWindowTitle("Add To Lineup");

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    _combo = new QComboBox(this);
    _combo->setEditable(true);
    form->addRow("Object:", _combo);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddToLineupDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AddToLineupDialog::reject);
    layout->addWidget(buttons);

    populateObjects();
}

QString AddToLineupDialog::selectedName() const
{
    if (!_combo) {
        return QString();
    }
    return _combo->currentText().trimmed();
}

void AddToLineupDialog::accept()
{
    const QString name = selectedName();
    if (name.isEmpty()) {
        QMessageBox::information(this, "Add To Lineup", "Please select an object or enter a name.");
        return;
    }

    QDialog::accept();
}

void AddToLineupDialog::populateObjects()
{
    if (!_combo || !_viewport) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return;
    }

    RenderObjIterator *iterator = asset_manager->Create_Render_Obj_Iterator();
    if (!iterator) {
        return;
    }

    for (iterator->First(); !iterator->Is_Done(); iterator->Next()) {
        const int class_id = iterator->Current_Item_Class_ID();
        if (!_viewport->canLineUpClass(class_id)) {
            continue;
        }
        const char *name = iterator->Current_Item_Name();
        if (name && name[0]) {
            _combo->addItem(QString::fromLatin1(name));
        }
    }

    asset_manager->Release_Render_Obj_Iterator(iterator);
}
