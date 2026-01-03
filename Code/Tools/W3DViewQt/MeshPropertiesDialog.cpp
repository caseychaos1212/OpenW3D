#include "MeshPropertiesDialog.h"

#include "assetmgr.h"
#include "mesh.h"
#include "meshmdl.h"
#include "rendobj.h"
#include "w3d_file.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
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

MeshPropertiesDialog::MeshPropertiesDialog(const QString &meshName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mesh Properties");

    auto *layout = new QVBoxLayout(this);

    _description = new QLabel(this);
    _description->setWordWrap(true);
    _description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_description);

    auto *form = new QFormLayout();
    _polygonCount = MakeValueLabel(this);
    _vertexCount = MakeValueLabel(this);
    _userText = MakeValueLabel(this);
    _userText->setWordWrap(true);

    form->addRow("Polygon Count:", _polygonCount);
    form->addRow("Vertex Count:", _vertexCount);
    form->addRow("User Text:", _userText);
    layout->addLayout(form);

    auto *mesh_type_group = new QGroupBox("Mesh Type", this);
    auto *mesh_type_layout = new QVBoxLayout(mesh_type_group);
    _meshTypeCollision = new QRadioButton("Collision Box", mesh_type_group);
    _meshTypeSkin = new QRadioButton("Skin", mesh_type_group);
    _meshTypeShadow = new QRadioButton("Shadow", mesh_type_group);
    _meshTypeNormal = new QRadioButton("Normal", mesh_type_group);
    mesh_type_layout->addWidget(_meshTypeCollision);
    mesh_type_layout->addWidget(_meshTypeSkin);
    mesh_type_layout->addWidget(_meshTypeShadow);
    mesh_type_layout->addWidget(_meshTypeNormal);
    layout->addWidget(mesh_type_group);

    auto *collision_group = new QGroupBox("Collision Type", this);
    auto *collision_layout = new QVBoxLayout(collision_group);
    _collisionPhysical = new QCheckBox("Physical", collision_group);
    _collisionProjectile = new QCheckBox("Projectile", collision_group);
    collision_layout->addWidget(_collisionPhysical);
    collision_layout->addWidget(_collisionProjectile);
    layout->addWidget(collision_group);

    _hiddenCheck = new QCheckBox("Hidden", this);
    layout->addWidget(_hiddenCheck);

    _meshTypeCollision->setEnabled(false);
    _meshTypeSkin->setEnabled(false);
    _meshTypeShadow->setEnabled(false);
    _meshTypeNormal->setEnabled(false);
    _collisionPhysical->setEnabled(false);
    _collisionProjectile->setEnabled(false);
    _hiddenCheck->setEnabled(false);

    auto *buttons_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons_box);

    if (meshName.isEmpty()) {
        setErrorState("No mesh selected.");
        return;
    }

    _description->setText(QString("Mesh: %1").arg(meshName));

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        setErrorState("WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = meshName.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        setErrorState("Failed to load mesh.");
        return;
    }

    _polygonCount->setText(QString::number(render_obj->Get_Num_Polys()));

    if (render_obj->Class_ID() != RenderObjClass::CLASSID_MESH) {
        setErrorState("Selected object is not a mesh.");
        render_obj->Release_Ref();
        return;
    }

    auto *mesh = static_cast<MeshClass *>(render_obj);
    MeshModelClass *model = mesh->Get_Model();
    if (model) {
        _vertexCount->setText(QString::number(model->Get_Vertex_Count()));
    }

    const char *user_text = mesh->Get_User_Text();
    if (user_text) {
        _userText->setText(QString::fromLatin1(user_text));
    } else {
        _userText->setText("");
    }

    const uint32 flags = mesh->Get_W3D_Flags();

    if ((flags & W3D_MESH_FLAG_COLLISION_BOX) == W3D_MESH_FLAG_COLLISION_BOX) {
        _meshTypeCollision->setChecked(true);
    } else if ((flags & W3D_MESH_FLAG_SKIN) == W3D_MESH_FLAG_SKIN) {
        _meshTypeSkin->setChecked(true);
    } else if ((flags & W3D_MESH_FLAG_SHADOW) == W3D_MESH_FLAG_SHADOW) {
        _meshTypeShadow->setChecked(true);
    } else {
        _meshTypeNormal->setChecked(true);
    }

    const uint32 collision_flags = flags & W3D_MESH_FLAG_COLLISION_TYPE_MASK;
    if ((collision_flags & W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL) == W3D_MESH_FLAG_COLLISION_TYPE_PHYSICAL) {
        _collisionPhysical->setChecked(true);
    }
    if ((collision_flags & W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE) ==
        W3D_MESH_FLAG_COLLISION_TYPE_PROJECTILE) {
        _collisionProjectile->setChecked(true);
    }

    if ((flags & W3D_MESH_FLAG_HIDDEN) == W3D_MESH_FLAG_HIDDEN) {
        _hiddenCheck->setChecked(true);
    }

    render_obj->Release_Ref();
}

void MeshPropertiesDialog::setErrorState(const QString &message)
{
    if (_description) {
        _description->setText(message);
    }
    if (_polygonCount) {
        _polygonCount->setText("n/a");
    }
    if (_vertexCount) {
        _vertexCount->setText("n/a");
    }
    if (_userText) {
        _userText->setText("");
    }
    if (_meshTypeNormal) {
        _meshTypeNormal->setChecked(false);
    }
    if (_meshTypeCollision) {
        _meshTypeCollision->setChecked(false);
    }
    if (_meshTypeSkin) {
        _meshTypeSkin->setChecked(false);
    }
    if (_meshTypeShadow) {
        _meshTypeShadow->setChecked(false);
    }
    if (_collisionPhysical) {
        _collisionPhysical->setChecked(false);
    }
    if (_collisionProjectile) {
        _collisionProjectile->setChecked(false);
    }
    if (_hiddenCheck) {
        _hiddenCheck->setChecked(false);
    }
}
