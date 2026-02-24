#include "MainWindow.h"

#include "RenderObjUtils.h"
#include "W3DViewport.h"

#include "agg_def.h"
#include "assetmgr.h"
#include "bmp2d.h"
#include "chunkio.h"
#include "dx8wrapper.h"
#include "ffactory.h"
#include "hanim.h"
#include "hmorphanim.h"
#include "hlod.h"
#include "htree.h"
#include "matrix3d.h"
#include "part_emt.h"
#include "part_ldr.h"
#include "quat.h"
#include "rawfile.h"
#include "refcount.h"
#include "rendobj.h"
#include "ringobj.h"
#include "shader.h"
#include "soundrobj.h"
#include "textfile.h"
#include "texture.h"
#include "sphereobj.h"
#include "vector3.h"
#include "v3_rnd.h"
#include "ww3d.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QMimeData>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QDropEvent>
#include <QVBoxLayout>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QVariant>
#include <QClipboard>
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>

#include "CameraDistanceDialog.h"
#include "CameraSettingsDialog.h"
#include "AddToLineupDialog.h"
#include "AdvancedAnimationDialog.h"
#include "AggregateNameDialog.h"
#include "AnimationPropertiesDialog.h"
#include "AnimatedSoundOptionsDialog.h"
#include "BackgroundObjectDialog.h"
#include "BoneManagementDialog.h"
#include "EmitterEditDialog.h"
#include "GammaDialog.h"
#include "HierarchyPropertiesDialog.h"
#include "MeshPropertiesDialog.h"
#include "RingEditDialog.h"
#include "ScaleDialog.h"
#include "SphereEditDialog.h"
#include "SoundEditDialog.h"
#include "TexturePathDialog.h"

namespace {
constexpr int kRoleType = Qt::UserRole + 1;
constexpr int kRoleName = Qt::UserRole + 2;
constexpr int kRoleHierarchyName = Qt::UserRole + 3;
constexpr int kRolePointer = Qt::UserRole + 4;
constexpr int kRoleClassId = Qt::UserRole + 5;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr int kMaxRecentFiles = 10;

enum class AssetNodeType {
    None = 0,
    Group = 1,
    RenderObject = 2,
    Animation = 3,
    Material = 4,
};

struct RenderObjInfo {
    bool isAggregate = false;
    bool isRealLod = false;
    QString hierarchyName;
};

enum class LodNamingType {
    Commando = 0,
    G = 1,
};

RenderObjInfo InspectRenderObj(const char *name)
{
    RenderObjInfo info;
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return info;
    }

    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name);
    if (!render_obj) {
        return info;
    }

    info.isAggregate = render_obj->Get_Base_Model_Name() != nullptr;
    if (render_obj->Class_ID() == RenderObjClass::CLASSID_HLOD) {
        auto *hlod = static_cast<HLodClass *>(render_obj);
        info.isRealLod = hlod->Get_LOD_Count() > 1;
    }

    const HTreeClass *tree = render_obj->Get_HTree();
    if (tree && tree->Get_Name()) {
        info.hierarchyName = QString::fromLatin1(tree->Get_Name());
    }

    render_obj->Release_Ref();
    return info;
}

bool IsLodNameValid(const QString &name, LodNamingType &type)
{
    if (name.size() < 2) {
        return false;
    }

    const QChar last = name.at(name.size() - 1);
    const QChar second_last = name.at(name.size() - 2);
    if ((second_last == 'L' || second_last == 'l') && last.isDigit()) {
        type = LodNamingType::Commando;
        return true;
    }

    if (last.isLetter()) {
        type = LodNamingType::G;
        return true;
    }

    return false;
}

bool IsModelPartOfLod(const QString &name, const QString &base, LodNamingType type)
{
    if (!name.startsWith(base)) {
        return false;
    }

    const QString extension = name.mid(base.size());
    if (type == LodNamingType::Commando) {
        return extension.size() == 2 &&
            (extension.at(0) == 'L' || extension.at(0) == 'l') &&
            extension.at(1).isDigit();
    }

    return extension.size() == 1 && extension.at(0).isLetter();
}

HLodPrototypeClass *GenerateLodPrototype(const QString &base_name, LodNamingType type)
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return nullptr;
    }

    RenderObjIterator *iterator = asset_manager->Create_Render_Obj_Iterator();
    if (!iterator) {
        return nullptr;
    }

    int lod_count = 0;
    int starting_index = std::numeric_limits<int>::max();
    QChar starting_char = 'Z';

    for (iterator->First(); !iterator->Is_Done(); iterator->Next()) {
        const char *item_name = iterator->Current_Item_Name();
        if (!item_name || !item_name[0]) {
            continue;
        }

        if (!asset_manager->Render_Obj_Exists(item_name)) {
            continue;
        }

        if (iterator->Current_Item_Class_ID() != RenderObjClass::CLASSID_HLOD) {
            continue;
        }

        const QString qname = QString::fromLatin1(item_name);
        if (!IsModelPartOfLod(qname, base_name, type)) {
            continue;
        }

        ++lod_count;
        const QChar last = qname.at(qname.size() - 1);
        if (type == LodNamingType::Commando) {
            starting_index = std::min(starting_index, last.digitValue());
        } else {
            const QChar upper = last.toUpper();
            if (upper < starting_char) {
                starting_char = upper;
            }
        }
    }

    asset_manager->Release_Render_Obj_Iterator(iterator);

    if (lod_count <= 0) {
        return nullptr;
    }

    if (type == LodNamingType::Commando && starting_index == std::numeric_limits<int>::max()) {
        return nullptr;
    }

    QVector<RenderObjClass *> lod_array(lod_count, nullptr);
    for (int lod_index = 0; lod_index < lod_count; ++lod_index) {
        QString lod_name;
        if (type == LodNamingType::Commando) {
            lod_name = QString("%1L%2").arg(base_name).arg(starting_index + lod_index);
        } else {
            lod_name = base_name + QChar(starting_char.unicode() + lod_index);
        }

        const QByteArray name_bytes = lod_name.toLatin1();
        RenderObjClass *lod_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
        if (!lod_obj) {
            for (auto *item : lod_array) {
                if (item) {
                    item->Release_Ref();
                }
            }
            return nullptr;
        }

        lod_array[lod_count - (lod_index + 1)] = lod_obj;
    }

    const QByteArray base_bytes = base_name.toLatin1();
    auto *new_lod = new HLodClass(base_bytes.constData(), lod_array.data(), lod_count);
    auto *definition = new HLodDefClass(*new_lod);
    auto *prototype = new HLodPrototypeClass(definition);

    new_lod->Release_Ref();
    for (auto *item : lod_array) {
        if (item) {
            item->Release_Ref();
        }
    }

    return prototype;
}

void CollectHierarchyItems(QStandardItem *parent,
                           const QString &hierarchyName,
                           QVector<QStandardItem *> &items)
{
    if (!parent || hierarchyName.isEmpty()) {
        return;
    }

    const int count = parent->rowCount();
    for (int index = 0; index < count; ++index) {
        auto *child = parent->child(index);
        if (!child) {
            continue;
        }

        const QString itemHierarchy = child->data(kRoleHierarchyName).toString();
        if (itemHierarchy == hierarchyName) {
            items.push_back(child);
        }
    }
}

void CollectAllChildren(QStandardItem *parent, QVector<QStandardItem *> &items)
{
    if (!parent) {
        return;
    }

    const int count = parent->rowCount();
    for (int index = 0; index < count; ++index) {
        auto *child = parent->child(index);
        if (child) {
            items.push_back(child);
        }
    }
}

void AdjustLightIntensity(Vector3 &color, float inc)
{
    color.X = std::clamp(color.X + inc, 0.0f, 1.0f);
    color.Y = std::clamp(color.Y + inc, 0.0f, 1.0f);
    color.Z = std::clamp(color.Z + inc, 0.0f, 1.0f);
}

QColor ToQColor(const Vector3 &color)
{
    return QColor::fromRgbF(color.X, color.Y, color.Z);
}

Vector3 ToVector3(const QColor &color)
{
    return Vector3(color.redF(), color.greenF(), color.blueF());
}

void SortAnimationChildren(QStandardItem *parent)
{
    if (!parent) {
        return;
    }

    const int count = parent->rowCount();
    for (int index = 0; index < count; ++index) {
        auto *child = parent->child(index);
        if (child) {
            child->sortChildren(0, Qt::AscendingOrder);
        }
    }
}

void SetHighestLod(RenderObjClass *render_obj)
{
    if (!render_obj) {
        return;
    }

    const int count = render_obj->Get_Num_Sub_Objects();
    for (int index = 0; index < count; ++index) {
        RenderObjClass *sub_obj = render_obj->Get_Sub_Object(index);
        if (sub_obj) {
            SetHighestLod(sub_obj);
            sub_obj->Release_Ref();
        }
    }

    if (render_obj->Class_ID() == RenderObjClass::CLASSID_HLOD) {
        auto *hlod = static_cast<HLodClass *>(render_obj);
        const int max_level = hlod->Get_LOD_Count() - 1;
        if (max_level >= 0) {
            hlod->Set_LOD_Level(max_level);
        }
    }
}

QAction *CreateWindowShortcutAction(QWidget *window, const QList<QKeySequence> &shortcuts)
{
    if (!window || shortcuts.isEmpty()) {
        return nullptr;
    }

    auto *action = new QAction(window);
    action->setShortcuts(shortcuts);
    action->setShortcutContext(Qt::WindowShortcut);
    window->addAction(action);
    return action;
}

bool GetSelectedRenderObject(QTreeView *tree, QString &name, int &class_id)
{
    if (!tree) {
        return false;
    }

    const QModelIndex current = tree->currentIndex();
    if (!current.isValid()) {
        return false;
    }

    if (current.data(kRoleType).toInt() != static_cast<int>(AssetNodeType::RenderObject)) {
        return false;
    }

    name = current.data(kRoleName).toString();
    class_id = current.data(kRoleClassId).toInt();
    return !name.isEmpty();
}

bool GetSelectedRenderObjectName(QTreeView *tree, QString &name)
{
    if (!tree) {
        return false;
    }

    QModelIndex current = tree->currentIndex();
    if (!current.isValid()) {
        return false;
    }

    const int type_value = current.data(kRoleType).toInt();
    if (type_value == static_cast<int>(AssetNodeType::RenderObject)) {
        name = current.data(kRoleName).toString();
        return !name.isEmpty();
    }

    if (type_value == static_cast<int>(AssetNodeType::Animation)) {
        QModelIndex render_index = current.parent();
        while (render_index.isValid() &&
               render_index.data(kRoleType).toInt() != static_cast<int>(AssetNodeType::RenderObject)) {
            render_index = render_index.parent();
        }
        if (!render_index.isValid()) {
            return false;
        }
        name = render_index.data(kRoleName).toString();
        return !name.isEmpty();
    }

    return false;
}

QString GetSelectedHierarchyName(QTreeView *tree)
{
    if (!tree) {
        return QString();
    }

    QModelIndex current = tree->currentIndex();
    while (current.isValid()) {
        const QString hierarchy = current.data(kRoleHierarchyName).toString();
        if (!hierarchy.isEmpty()) {
            return hierarchy;
        }
        current = current.parent();
    }

    return QString();
}

QModelIndex FindRenderObjectIndex(QStandardItemModel *model, const QString &name, int class_id)
{
    if (!model || name.isEmpty() || model->rowCount() <= 0) {
        return QModelIndex();
    }

    const QModelIndex start = model->index(0, 0);
    const QModelIndexList matches = model->match(start,
                                                 kRoleName,
                                                 name,
                                                 -1,
                                                 Qt::MatchExactly | Qt::MatchRecursive);
    for (const QModelIndex &match : matches) {
        if (!match.isValid()) {
            continue;
        }
        if (match.data(kRoleType).toInt() != static_cast<int>(AssetNodeType::RenderObject)) {
            continue;
        }
        if (class_id >= 0 && match.data(kRoleClassId).toInt() != class_id) {
            continue;
        }
        return match;
    }

    return QModelIndex();
}

void ExpandParentChain(QTreeView *tree, QModelIndex index)
{
    if (!tree) {
        return;
    }

    while (index.isValid()) {
        tree->expand(index);
        index = index.parent();
    }
}

QString ResolveGroupLabel(QStandardItemModel *model, const QModelIndex &index, bool is_group)
{
    if (!model || !index.isValid()) {
        return QString();
    }

    auto *item = model->itemFromIndex(index);
    if (!item) {
        return QString();
    }

    if (is_group) {
        return item->text();
    }

    if (auto *parent = item->parent()) {
        return parent->text();
    }

    return QString();
}

QString FindHierarchyAssetPath(const QString &directory, const QString &hierarchy)
{
    if (directory.isEmpty() || hierarchy.isEmpty()) {
        return QString();
    }

    QDir dir(directory);
    QString base = hierarchy;
    if (base.endsWith(".w3d", Qt::CaseInsensitive)) {
        base.chop(4);
    }

    const QString direct_path = dir.filePath(base + ".w3d");
    if (QFileInfo::exists(direct_path)) {
        return direct_path;
    }

    const auto entries = dir.entryInfoList(QStringList() << "*.w3d" << "*.W3D", QDir::Files);
    for (const auto &info : entries) {
        if (info.completeBaseName().compare(base, Qt::CaseInsensitive) == 0) {
            return info.absoluteFilePath();
        }
    }

    return QString();
}

void LoadMissingHierarchyAssets(WW3DAssetManager *asset_manager, const QString &directory)
{
    if (!asset_manager || directory.isEmpty()) {
        return;
    }

    QSet<QString> loaded_hierarchies;
    RenderObjIterator *render_iter = asset_manager->Create_Render_Obj_Iterator();
    if (render_iter) {
        for (render_iter->First(); !render_iter->Is_Done(); render_iter->Next()) {
            const char *name = render_iter->Current_Item_Name();
            if (!name || !name[0]) {
                continue;
            }

            const RenderObjInfo info = InspectRenderObj(name);
            if (!info.hierarchyName.isEmpty()) {
                loaded_hierarchies.insert(info.hierarchyName.toUpper());
            }
        }
        asset_manager->Release_Render_Obj_Iterator(render_iter);
    }

    QSet<QString> anim_hierarchies;
    AssetIterator *anim_iter = asset_manager->Create_HAnim_Iterator();
    if (anim_iter) {
        for (anim_iter->First(); !anim_iter->Is_Done(); anim_iter->Next()) {
            const char *anim_name = anim_iter->Current_Item_Name();
            if (!anim_name || !anim_name[0]) {
                continue;
            }

            HAnimClass *anim = asset_manager->Get_HAnim(anim_name);
            if (!anim) {
                continue;
            }

            const char *hier_name = anim->Get_HName();
            if (hier_name && hier_name[0]) {
                anim_hierarchies.insert(QString::fromLatin1(hier_name).toUpper());
            }
            anim->Release_Ref();
        }
        delete anim_iter;
    }

    for (const auto &hierarchy : anim_hierarchies) {
        if (loaded_hierarchies.contains(hierarchy)) {
            continue;
        }

        const QString path = FindHierarchyAssetPath(directory, hierarchy);
        if (path.isEmpty()) {
            continue;
        }

        const QByteArray path_bytes = QDir::toNativeSeparators(path).toLocal8Bit();
        if (asset_manager->Load_3D_Assets(path_bytes.constData())) {
            loaded_hierarchies.insert(hierarchy);
        }
    }
}

bool ImportFacialAnimation(const QString &hierarchy, const QString &path)
{
    if (hierarchy.isEmpty() || path.isEmpty()) {
        return false;
    }

    const QByteArray file_native = QDir::toNativeSeparators(path).toLocal8Bit();
    TextFileClass anim_desc_file(file_native.constData());
    if (anim_desc_file.Open() != true) {
        return false;
    }

    HMorphAnimClass *new_anim = new HMorphAnimClass;
    const QByteArray hierarchy_bytes = hierarchy.toLatin1();
    if (!new_anim->Import(hierarchy_bytes.constData(), anim_desc_file)) {
        anim_desc_file.Close();
        new_anim->Release_Ref();
        return false;
    }

    const QString anim_name = QFileInfo(path).completeBaseName().toUpper();
    const QString new_name = QString("%1.%2").arg(hierarchy, anim_name);
    const QByteArray new_name_bytes = new_name.toLatin1();
    new_anim->Set_Name(new_name_bytes.constData());

    if (auto *asset_manager = WW3DAssetManager::Get_Instance()) {
        asset_manager->Add_Anim(new_anim);
    }

    const QString output_path = QDir(QFileInfo(path).absolutePath()).filePath(anim_name + ".w3d");
    const QByteArray output_native = QDir::toNativeSeparators(output_path).toLocal8Bit();
    RawFileClass animation_file(output_native.constData());
    if (animation_file.Create() == (int)true &&
        animation_file.Open(FileClass::WRITE) == (int)true) {
        ChunkSaveClass csave(&animation_file);
        new_anim->Save_W3D(csave);
        animation_file.Close();
    }

    anim_desc_file.Close();
    new_anim->Release_Ref();
    return true;
}

bool SaveChunkToFile(const QString &path, const std::function<bool(ChunkSaveClass &)> &save_fn)
{
    if (path.isEmpty() || !_TheFileFactory) {
        return false;
    }

    const QByteArray native = QDir::toNativeSeparators(path).toLocal8Bit();
    FileClass *file = _TheFileFactory->Get_File(native.constData());
    if (!file) {
        return false;
    }

    file->Open(FileClass::WRITE);
    ChunkSaveClass save_chunk(file);
    const bool ok = save_fn(save_chunk);
    file->Close();
    _TheFileFactory->Return_File(file);
    return ok;
}

bool UpdateSoundPrototype(SoundRenderObjClass *sound_obj, const QString &old_name)
{
    if (!sound_obj) {
        return false;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return false;
    }

    SoundRenderObjDefClass *definition = new SoundRenderObjDefClass(*sound_obj);
    SoundRenderObjPrototypeClass *prototype = new SoundRenderObjPrototypeClass(definition);

    if (!old_name.isEmpty()) {
        const QByteArray old_bytes = old_name.toLatin1();
        asset_manager->Remove_Prototype(old_bytes.constData());
    }

    asset_manager->Add_Prototype(prototype);
    return true;
}

ParticleEmitterDefClass CreateDefaultEmitterDefinition()
{
    ParticlePropertyStruct<Vector3> color;
    color.Start = Vector3(1, 1, 1);
    color.Rand.Set(0, 0, 0);
    color.NumKeyFrames = 0;
    color.KeyTimes = nullptr;
    color.Values = nullptr;

    ParticlePropertyStruct<float> opacity;
    opacity.Start = 1.0f;
    opacity.Rand = 0.0f;
    opacity.NumKeyFrames = 0;
    opacity.KeyTimes = nullptr;
    opacity.Values = nullptr;

    ParticlePropertyStruct<float> size;
    size.Start = 0.1f;
    size.Rand = 0.0f;
    size.NumKeyFrames = 0;
    size.KeyTimes = nullptr;
    size.Values = nullptr;

    ParticlePropertyStruct<float> rotation;
    rotation.Start = 0.0f;
    rotation.Rand = 0.0f;
    rotation.NumKeyFrames = 0;
    rotation.KeyTimes = nullptr;
    rotation.Values = nullptr;

    ParticlePropertyStruct<float> frames;
    frames.Start = 0.0f;
    frames.Rand = 0.0f;
    frames.NumKeyFrames = 0;
    frames.KeyTimes = nullptr;
    frames.Values = nullptr;

    ParticlePropertyStruct<float> blur_times;
    blur_times.Start = 0.0f;
    blur_times.Rand = 0.0f;
    blur_times.NumKeyFrames = 0;
    blur_times.KeyTimes = nullptr;
    blur_times.Values = nullptr;

    auto *emitter = new ParticleEmitterClass(
        10.0f,
        1,
        new Vector3SolidBoxRandomizer(Vector3(0.1f, 0.1f, 0.1f)),
        Vector3(0, 0, 1),
        new Vector3SolidBoxRandomizer(Vector3(0, 0, 0.1f)),
        0.0f,
        0.0f,
        color,
        opacity,
        size,
        rotation,
        0.0f,
        frames,
        blur_times,
        Vector3(0, 0, 0),
        1.0f,
        nullptr,
        ShaderClass::_PresetAdditiveSpriteShader,
        0);

    ParticleEmitterDefClass *definition = emitter->Build_Definition();
    ParticleEmitterDefClass copy;
    if (definition) {
        copy = *definition;
        delete definition;
    }
    emitter->Release_Ref();
    return copy;
}

bool UpdateEmitterPrototype(const ParticleEmitterDefClass &definition, const QString &old_name)
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return false;
    }

    auto *definition_copy = new ParticleEmitterDefClass(definition);
    auto *prototype = new ParticleEmitterPrototypeClass(definition_copy);

    if (!old_name.isEmpty()) {
        const QByteArray old_bytes = old_name.toLatin1();
        asset_manager->Remove_Prototype(old_bytes.constData());
    }

    asset_manager->Add_Prototype(prototype);
    return true;
}

bool UpdateSpherePrototype(SphereRenderObjClass *sphere, const QString &old_name)
{
    if (!sphere) {
        return false;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return false;
    }

    auto *prototype = new SpherePrototypeClass(sphere);
    if (!old_name.isEmpty()) {
        const QByteArray old_bytes = old_name.toLatin1();
        asset_manager->Remove_Prototype(old_bytes.constData());
    }

    asset_manager->Add_Prototype(prototype);
    return true;
}

bool UpdateRingPrototype(RingRenderObjClass *ring, const QString &old_name)
{
    if (!ring) {
        return false;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return false;
    }

    auto *prototype = new RingPrototypeClass(ring);
    if (!old_name.isEmpty()) {
        const QByteArray old_bytes = old_name.toLatin1();
        asset_manager->Remove_Prototype(old_bytes.constData());
    }

    asset_manager->Add_Prototype(prototype);
    return true;
}
} // namespace

W3DViewMainWindow::W3DViewMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("W3DViewQt");
    setAcceptDrops(true);

    auto *file_menu = menuBar()->addMenu("&File");
    _newAction = file_menu->addAction("&New");
    _newAction->setShortcut(QKeySequence::New);
    connect(_newAction, &QAction::triggered, this, &W3DViewMainWindow::newFile);
    _openAction = file_menu->addAction("&Open...");
    _openAction->setShortcut(QKeySequence::Open);
    connect(_openAction, &QAction::triggered, this, &W3DViewMainWindow::openFile);
    _mungeSortAction = file_menu->addAction("&Munge Sort on Load");
    _mungeSortAction->setCheckable(true);
    connect(_mungeSortAction, &QAction::triggered, this, &W3DViewMainWindow::toggleMungeSortOnLoad);
    _enableGammaAction = file_menu->addAction("&Enable Gamma Correction");
    _enableGammaAction->setCheckable(true);
    connect(_enableGammaAction, &QAction::triggered, this, &W3DViewMainWindow::toggleGammaCorrection);
    file_menu->addSeparator();
    _saveSettingsAction = file_menu->addAction("&Save Settings...");
    connect(_saveSettingsAction, &QAction::triggered, this, &W3DViewMainWindow::saveSettingsFile);
    _saveSettingsAction->setShortcut(QKeySequence::Save);
    _loadSettingsAction = file_menu->addAction("Load &Settings...");
    connect(_loadSettingsAction, &QAction::triggered, this, &W3DViewMainWindow::loadSettingsFile);
    file_menu->addSeparator();
    auto *import_facial_action = file_menu->addAction("&Import Facial Anims...");
    connect(import_facial_action, &QAction::triggered, this, &W3DViewMainWindow::importFacialAnims);
    auto *export_menu = file_menu->addMenu("Ex&port...");
    _exportAggregateAction = export_menu->addAction("&Aggregate...");
    connect(_exportAggregateAction, &QAction::triggered, this, &W3DViewMainWindow::exportAggregate);
    _exportEmitterAction = export_menu->addAction("&Emitter...");
    connect(_exportEmitterAction, &QAction::triggered, this, &W3DViewMainWindow::exportEmitter);
    _exportEmitterAction->setEnabled(false);
    _exportLodAction = export_menu->addAction("&LOD...");
    connect(_exportLodAction, &QAction::triggered, this, &W3DViewMainWindow::exportLod);
    _exportPrimitiveAction = export_menu->addAction("&Primitive...");
    connect(_exportPrimitiveAction, &QAction::triggered, this, &W3DViewMainWindow::exportPrimitive);
    _exportPrimitiveAction->setEnabled(false);
    _exportSoundObjectAction = export_menu->addAction("&Sound Object...");
    connect(_exportSoundObjectAction, &QAction::triggered, this, &W3DViewMainWindow::exportSoundObject);
    _exportSoundObjectAction->setEnabled(false);
    file_menu->addSeparator();
    auto *texture_path_action = file_menu->addAction("&Texture Path...");
    connect(texture_path_action, &QAction::triggered, this, &W3DViewMainWindow::openTexturePathsDialog);
    auto *animated_sound_action = file_menu->addAction("&Animated Sound Options...");
    connect(animated_sound_action, &QAction::triggered, this, &W3DViewMainWindow::openAnimatedSoundOptions);
    file_menu->addSeparator();
    _recentFilesMenu = file_menu->addMenu("Recent File");
    updateRecentFilesMenu();
    file_menu->addSeparator();
    file_menu->addAction("E&xit", this, &QWidget::close);

    auto *settings_menu = menuBar()->addMenu("&Settings");
    _texturePathsAction = settings_menu->addAction("&Texture Paths...");
    connect(_texturePathsAction, &QAction::triggered, this, &W3DViewMainWindow::openTexturePathsDialog);
    _autoExpandTreeAction = settings_menu->addAction("&Auto Expand Asset Tree");
    _autoExpandTreeAction->setCheckable(true);
    connect(_autoExpandTreeAction, &QAction::toggled, this, &W3DViewMainWindow::toggleAutoExpandAssetTree);

    auto *view_menu = menuBar()->addMenu("&View");
    auto *toolbars_menu = view_menu->addMenu("&Toolbars");
    _toolbarMainAction = toolbars_menu->addAction("&Main");
    _toolbarMainAction->setCheckable(true);
    _toolbarMainAction->setChecked(true);
    connect(_toolbarMainAction, &QAction::toggled, this, &W3DViewMainWindow::toggleMainToolbar);
    _toolbarObjectAction = toolbars_menu->addAction("Object");
    _toolbarObjectAction->setCheckable(true);
    _toolbarObjectAction->setChecked(true);
    connect(_toolbarObjectAction, &QAction::toggled, this, &W3DViewMainWindow::toggleObjectToolbar);
    _toolbarAnimationAction = toolbars_menu->addAction("Animation");
    _toolbarAnimationAction->setCheckable(true);
    _toolbarAnimationAction->setChecked(false);
    connect(_toolbarAnimationAction, &QAction::toggled, this, &W3DViewMainWindow::toggleAnimationToolbar);
    _statusBarAction = view_menu->addAction("&Status Bar");
    _statusBarAction->setCheckable(true);
    _statusBarAction->setChecked(true);
    connect(_statusBarAction, &QAction::triggered, this, &W3DViewMainWindow::toggleStatusBar);
    view_menu->addSeparator();
    _slideshowPrevAction = view_menu->addAction("&Prev");
    _slideshowPrevAction->setShortcut(QKeySequence(Qt::Key_PageUp));
    connect(_slideshowPrevAction, &QAction::triggered, this, &W3DViewMainWindow::selectPrevAsset);
    _slideshowNextAction = view_menu->addAction("&Next");
    _slideshowNextAction->setShortcut(QKeySequence(Qt::Key_PageDown));
    connect(_slideshowNextAction, &QAction::triggered, this, &W3DViewMainWindow::selectNextAsset);
    view_menu->addSeparator();
    _wireframeAction = view_menu->addAction("&Wireframe Mode");
    _wireframeAction->setCheckable(true);
    connect(_wireframeAction, &QAction::triggered, this, &W3DViewMainWindow::toggleWireframe);
    _sortingAction = view_menu->addAction("Polygon Sorting");
    _sortingAction->setCheckable(true);
    _sortingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(_sortingAction, &QAction::triggered, this, &W3DViewMainWindow::toggleSorting);
    _invertBackfaceCullingAction = view_menu->addAction("Invert Backface Culling");
    _invertBackfaceCullingAction->setCheckable(true);
    connect(_invertBackfaceCullingAction, &QAction::triggered,
            this, &W3DViewMainWindow::toggleBackfaceCulling);
    _gammaAction = view_menu->addAction("Set &Gamma");
    connect(_gammaAction, &QAction::triggered, this, &W3DViewMainWindow::openGammaDialog);
    view_menu->addSeparator();
    auto *npatches_menu = view_menu->addMenu("N-Patches Subdivision Level");
    _npatchesGroup = new QActionGroup(this);
    _npatchesGroup->setExclusive(true);
    for (int level = 1; level <= 8; ++level) {
        auto *action = npatches_menu->addAction(QString::number(level));
        action->setCheckable(true);
        action->setData(level);
        _npatchesGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, level]() { setNpatchesLevel(level); });
    }
    _npatchesGapAction = view_menu->addAction("N-Patches Gap Filling");
    _npatchesGapAction->setCheckable(true);
    connect(_npatchesGapAction, &QAction::triggered, this, &W3DViewMainWindow::toggleNpatchesGap);

    auto *object_menu = menuBar()->addMenu("&Object");
    _objectMenuAction = object_menu->menuAction();
    _objectRotateXAction = object_menu->addAction("Rotate &X");
    _objectRotateXAction->setCheckable(true);
    _objectRotateXAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
    connect(_objectRotateXAction, &QAction::triggered, this, &W3DViewMainWindow::toggleObjectRotateX);
    _objectRotateYAction = object_menu->addAction("Rotate &Y");
    _objectRotateYAction->setCheckable(true);
    _objectRotateYAction->setShortcuts(
        QList<QKeySequence>{QKeySequence(Qt::Key_Up), QKeySequence(Qt::CTRL | Qt::Key_Y)});
    connect(_objectRotateYAction, &QAction::triggered, this, &W3DViewMainWindow::toggleObjectRotateY);
    _objectRotateZAction = object_menu->addAction("Rotate &Z");
    _objectRotateZAction->setCheckable(true);
    _objectRotateZAction->setShortcuts(
        QList<QKeySequence>{QKeySequence(Qt::Key_Right), QKeySequence(Qt::CTRL | Qt::Key_Z)});
    connect(_objectRotateZAction, &QAction::triggered, this, &W3DViewMainWindow::toggleObjectRotateZ);
    object_menu->addSeparator();
    _objectPropertiesAction = object_menu->addAction("&Properties...");
    _objectPropertiesAction->setShortcut(QKeySequence(Qt::Key_Return));
    connect(_objectPropertiesAction, &QAction::triggered, this, &W3DViewMainWindow::showObjectProperties);
    object_menu->addSeparator();
    _restrictAnimsAction = object_menu->addAction("&Restrict Anims");
    _restrictAnimsAction->setCheckable(true);
    connect(_restrictAnimsAction, &QAction::triggered, this, &W3DViewMainWindow::toggleRestrictAnims);
    object_menu->addSeparator();
    _objectResetAction = object_menu->addAction("&Reset");
    connect(_objectResetAction, &QAction::triggered, this, &W3DViewMainWindow::resetObject);
    object_menu->addSeparator();
    _objectAlternateAction = object_menu->addAction("Toggle Alternate Materials");
    connect(_objectAlternateAction, &QAction::triggered, this, &W3DViewMainWindow::toggleAlternateMaterials);

    _animationMenu = new QMenu("&Animation", this);
    _animationPlayAction = _animationMenu->addAction("&Play");
    connect(_animationPlayAction, &QAction::triggered, this, &W3DViewMainWindow::startAnimation);
    _animationPauseAction = _animationMenu->addAction("P&ause");
    connect(_animationPauseAction, &QAction::triggered, this, &W3DViewMainWindow::pauseAnimation);
    _animationStopAction = _animationMenu->addAction("&Stop");
    connect(_animationStopAction, &QAction::triggered, this, &W3DViewMainWindow::stopAnimation);
    _animationMenu->addSeparator();
    _animationStepBackAction = _animationMenu->addAction("Step &Back");
    connect(_animationStepBackAction, &QAction::triggered, this, &W3DViewMainWindow::stepAnimationBackward);
    _animationStepForwardAction = _animationMenu->addAction("Step &Forward");
    connect(_animationStepForwardAction, &QAction::triggered, this, &W3DViewMainWindow::stepAnimationForward);
    _animationMenu->addSeparator();
    auto *animation_settings_action = _animationMenu->addAction("Se&ttings");
    connect(animation_settings_action, &QAction::triggered, this, &W3DViewMainWindow::openAnimationSettings);
    _animationMenu->addSeparator();
    auto *animation_advanced_action = _animationMenu->addAction("Ad&vanced...");
    connect(animation_advanced_action, &QAction::triggered, this, &W3DViewMainWindow::openAdvancedAnimation);
    connect(_animationMenu, &QMenu::aboutToShow, this, &W3DViewMainWindow::refreshAnimationMenu);

    _hierarchyMenu = new QMenu("&Hierarchy", this);
    auto *hierarchy_generate_action = _hierarchyMenu->addAction("&Generate LOD...");
    connect(hierarchy_generate_action, &QAction::triggered, this, &W3DViewMainWindow::generateLod);
    auto *hierarchy_aggregate_action = _hierarchyMenu->addAction("&Make Aggregate...");
    connect(hierarchy_aggregate_action, &QAction::triggered, this, &W3DViewMainWindow::makeAggregate);

    _aggregateMenu = new QMenu("&Aggregate", this);
    auto *aggregate_rename_action = _aggregateMenu->addAction("R&ename Aggregate...");
    connect(aggregate_rename_action, &QAction::triggered, this, &W3DViewMainWindow::renameAggregate);
    _aggregateMenu->addSeparator();
    auto *aggregate_bone_action = _aggregateMenu->addAction("&Bone Management...");
    connect(aggregate_bone_action, &QAction::triggered, this, &W3DViewMainWindow::openBoneManagement);
    auto *aggregate_auto_assign_action = _aggregateMenu->addAction("&Auto Assign Bone Models");
    connect(aggregate_auto_assign_action, &QAction::triggered, this, &W3DViewMainWindow::autoAssignBoneModels);
    _aggregateMenu->addSeparator();
    _aggregateBindSubobjectAction = _aggregateMenu->addAction("Bind &Subobject LOD");
    _aggregateBindSubobjectAction->setCheckable(true);
    connect(_aggregateBindSubobjectAction, &QAction::triggered, this, &W3DViewMainWindow::bindSubobjectLod);
    auto *aggregate_generate_action = _aggregateMenu->addAction("&Generate LOD...");
    connect(aggregate_generate_action, &QAction::triggered, this, &W3DViewMainWindow::generateLod);
    connect(_aggregateMenu, &QMenu::aboutToShow, this, &W3DViewMainWindow::refreshAggregateMenu);

    _lodMenu = new QMenu("&LOD", this);
    _lodRecordAction = _lodMenu->addAction("&Record Screen Area");
    connect(_lodRecordAction, &QAction::triggered, this, &W3DViewMainWindow::recordLodScreenArea);
    _lodIncludeNullAction = _lodMenu->addAction("Include &NULL Object");
    _lodIncludeNullAction->setCheckable(true);
    connect(_lodIncludeNullAction, &QAction::triggered, this, &W3DViewMainWindow::toggleLodIncludeNull);
    _lodMenu->addSeparator();
    _lodPrevAction = _lodMenu->addAction("&Prev Level");
    connect(_lodPrevAction, &QAction::triggered, this, &W3DViewMainWindow::selectPrevLod);
    _lodNextAction = _lodMenu->addAction("&Next Level");
    connect(_lodNextAction, &QAction::triggered, this, &W3DViewMainWindow::selectNextLod);
    _lodAutoSwitchAction = _lodMenu->addAction("&Auto Switching");
    _lodAutoSwitchAction->setCheckable(true);
    connect(_lodAutoSwitchAction, &QAction::triggered, this, &W3DViewMainWindow::toggleLodAutoSwitch);
    _lodMenu->addSeparator();
    auto *lod_make_aggregate_action = _lodMenu->addAction("&Make Aggregate...");
    connect(lod_make_aggregate_action, &QAction::triggered, this, &W3DViewMainWindow::makeAggregate);
    connect(_lodMenu, &QMenu::aboutToShow, this, &W3DViewMainWindow::refreshLodMenu);

    auto *emitters_menu = menuBar()->addMenu("&Emitters");
    emitters_menu->addAction("&Create Emitter...", this, &W3DViewMainWindow::createEmitter);
    _scaleEmitterAction = emitters_menu->addAction("&Scale Emitter...");
    connect(_scaleEmitterAction, &QAction::triggered, this, &W3DViewMainWindow::scaleEmitter);
    _scaleEmitterAction->setEnabled(false);
    emitters_menu->addSeparator();
    _editEmitterAction = emitters_menu->addAction("&Edit Emitter");
    connect(_editEmitterAction, &QAction::triggered, this, &W3DViewMainWindow::editEmitter);
    _editEmitterAction->setEnabled(false);
    _emittersEditMenu = emitters_menu->addMenu("E&dit");
    connect(_emittersEditMenu, &QMenu::aboutToShow, this, &W3DViewMainWindow::updateEmittersEditMenu);

    auto *primitives_menu = menuBar()->addMenu("&Primitives");
    primitives_menu->addAction("Create &Sphere...", this, &W3DViewMainWindow::createSphere);
    primitives_menu->addAction("Create &Ring...", this, &W3DViewMainWindow::createRing);
    primitives_menu->addSeparator();
    _editPrimitiveAction = primitives_menu->addAction("&Edit Primitive...", this, &W3DViewMainWindow::editPrimitive);
    _editPrimitiveAction->setEnabled(false);

    auto *sound_menu = menuBar()->addMenu("&Sound");
    sound_menu->addAction("&Create Sound Object...", this, &W3DViewMainWindow::createSoundObject);
    sound_menu->addSeparator();
    _editSoundObjectAction = sound_menu->addAction("&Edit Sound Object...");
    connect(_editSoundObjectAction, &QAction::triggered, this, &W3DViewMainWindow::editSoundObject);
    _editSoundObjectAction->setEnabled(false);

    auto *lighting_menu = menuBar()->addMenu("Ligh&ting");
    _lightRotateYAction = lighting_menu->addAction("Rotate &Y");
    _lightRotateYAction->setCheckable(true);
    _lightRotateYAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
    connect(_lightRotateYAction, &QAction::triggered, this, &W3DViewMainWindow::toggleLightRotateY);
    _lightRotateZAction = lighting_menu->addAction("Rotate &Z");
    _lightRotateZAction->setCheckable(true);
    _lightRotateZAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
    connect(_lightRotateZAction, &QAction::triggered, this, &W3DViewMainWindow::toggleLightRotateZ);
    lighting_menu->addSeparator();
    lighting_menu->addAction("&Ambient...", this, &W3DViewMainWindow::setAmbientLight);
    lighting_menu->addAction("&Scene Light...", this, &W3DViewMainWindow::setSceneLight);
    lighting_menu->addSeparator();
    auto *inc_ambient_action = lighting_menu->addAction("&Inc Ambient Intensity", this,
                                                        &W3DViewMainWindow::increaseAmbientLight);
    inc_ambient_action->setShortcuts(
        QList<QKeySequence>{QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_Equal)});
    auto *dec_ambient_action = lighting_menu->addAction("&Dec Ambient Intensity", this,
                                                        &W3DViewMainWindow::decreaseAmbientLight);
    dec_ambient_action->setShortcut(QKeySequence(Qt::Key_Minus));
    auto *inc_scene_action = lighting_menu->addAction("Inc Scene &Light Intensity", this,
                                                      &W3DViewMainWindow::increaseSceneLight);
    inc_scene_action->setShortcuts(
        QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_Plus),
                            QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    auto *dec_scene_action = lighting_menu->addAction("De&c Scene Light Intensity", this,
                                                      &W3DViewMainWindow::decreaseSceneLight);
    dec_scene_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    lighting_menu->addSeparator();
    _exposePrelitAction = lighting_menu->addAction("Expose Precalculated Lighting");
    _exposePrelitAction->setCheckable(true);
    connect(_exposePrelitAction, &QAction::triggered, this, &W3DViewMainWindow::toggleExposePrelit);
    auto *kill_scene_light_action = lighting_menu->addAction("Kill Scene Light", this,
                                                             &W3DViewMainWindow::killSceneLight);
    kill_scene_light_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Asterisk));
    lighting_menu->addSeparator();
    _prelitGroup = new QActionGroup(this);
    _prelitVertexAction = lighting_menu->addAction("&Vertex Lighting");
    _prelitVertexAction->setCheckable(true);
    _prelitGroup->addAction(_prelitVertexAction);
    connect(_prelitVertexAction, &QAction::triggered, this, &W3DViewMainWindow::setPrelitVertex);
    _prelitMultipassAction = lighting_menu->addAction("Multi-&Pass Lighting");
    _prelitMultipassAction->setCheckable(true);
    _prelitGroup->addAction(_prelitMultipassAction);
    connect(_prelitMultipassAction, &QAction::triggered, this, &W3DViewMainWindow::setPrelitMultipass);
    _prelitMultitexAction = lighting_menu->addAction("Multi-Te&xture Lighting");
    _prelitMultitexAction->setCheckable(true);
    _prelitGroup->addAction(_prelitMultitexAction);
    connect(_prelitMultitexAction, &QAction::triggered, this, &W3DViewMainWindow::setPrelitMultitex);

    auto *camera_menu = menuBar()->addMenu("&Camera");
    _cameraFrontAction = camera_menu->addAction("&Front");
    _cameraFrontAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(_cameraFrontAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraFront);
    _cameraBackAction = camera_menu->addAction("&Back");
    _cameraBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(_cameraBackAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraBack);
    _cameraLeftAction = camera_menu->addAction("&Left");
    _cameraLeftAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(_cameraLeftAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraLeft);
    _cameraRightAction = camera_menu->addAction("&Right");
    _cameraRightAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(_cameraRightAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraRight);
    _cameraTopAction = camera_menu->addAction("&Top");
    _cameraTopAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(_cameraTopAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraTop);
    _cameraBottomAction = camera_menu->addAction("Bo&ttom");
    _cameraBottomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(_cameraBottomAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraBottom);
    camera_menu->addSeparator();
    _cameraRotateXAction = camera_menu->addAction("Rotate &X Only");
    _cameraRotateXAction->setCheckable(true);
    connect(_cameraRotateXAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraRotateX);
    _cameraRotateYAction = camera_menu->addAction("Rotate &Y Only");
    _cameraRotateYAction->setCheckable(true);
    connect(_cameraRotateYAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraRotateY);
    _cameraRotateZAction = camera_menu->addAction("Rotate &Z Only");
    _cameraRotateZAction->setCheckable(true);
    connect(_cameraRotateZAction, &QAction::triggered, this, &W3DViewMainWindow::setCameraRotateZ);
    _cameraCopyScreenAction = camera_menu->addAction("&Copy Screen Size To Clipboard");
    _cameraCopyScreenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    connect(_cameraCopyScreenAction, &QAction::triggered, this, &W3DViewMainWindow::copyScreenSize);
    camera_menu->addSeparator();
    _cameraAnimateAction = camera_menu->addAction("&Animate Camera");
    _cameraAnimateAction->setCheckable(true);
    _cameraAnimateAction->setShortcut(QKeySequence(Qt::Key_F8));
    connect(_cameraAnimateAction, &QAction::triggered, this, &W3DViewMainWindow::toggleCameraAnimate);
    _cameraBonePosXAction = camera_menu->addAction("+X Camera");
    _cameraBonePosXAction->setCheckable(true);
    connect(_cameraBonePosXAction, &QAction::triggered, this, &W3DViewMainWindow::toggleCameraBonePosX);
    camera_menu->addSeparator();
    _cameraSettingsAction = camera_menu->addAction("Settin&gs...");
    connect(_cameraSettingsAction, &QAction::triggered, this, &W3DViewMainWindow::openCameraSettings);
    _cameraDistanceAction = camera_menu->addAction("&Set Distance...");
    _cameraDistanceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(_cameraDistanceAction, &QAction::triggered, this, &W3DViewMainWindow::openCameraDistance);
    camera_menu->addSeparator();
    _cameraResetOnDisplayAction = camera_menu->addAction("Reset on &Display");
    _cameraResetOnDisplayAction->setCheckable(true);
    connect(_cameraResetOnDisplayAction, &QAction::triggered, this, &W3DViewMainWindow::toggleCameraResetOnDisplay);
    _cameraResetAction = camera_menu->addAction("R&eset");
    connect(_cameraResetAction, &QAction::triggered, this, &W3DViewMainWindow::resetCamera);

    auto *background_menu = menuBar()->addMenu("&Background");
    background_menu->addAction("&Color...", this, &W3DViewMainWindow::setBackgroundColor);
    background_menu->addAction("&Bitmap...", this, &W3DViewMainWindow::setBackgroundBitmap);
    _backgroundObjectAction = background_menu->addAction("&Object...");
    connect(_backgroundObjectAction, &QAction::triggered, this,
            &W3DViewMainWindow::openBackgroundObjectDialog);
    background_menu->addSeparator();
    _fogAction = background_menu->addAction("Fog", this, &W3DViewMainWindow::toggleFog);
    _fogAction->setCheckable(true);
    _fogAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));

    auto *movie_menu = menuBar()->addMenu("&Movie");
    _makeMovieAction = movie_menu->addAction("&Make Movie...");
    _makeMovieAction->setEnabled(false);
    connect(_makeMovieAction, &QAction::triggered, this, &W3DViewMainWindow::makeMovie);
    _captureScreenshotAction = movie_menu->addAction("&Capture Screen Shot...");
    _captureScreenshotAction->setShortcut(QKeySequence(Qt::Key_F7));
    connect(_captureScreenshotAction, &QAction::triggered, this,
            &W3DViewMainWindow::captureScreenshot);

    auto *help_menu = menuBar()->addMenu("&Help");
    _aboutAction = help_menu->addAction("&About...");
    connect(_aboutAction, &QAction::triggered, this, &W3DViewMainWindow::showAbout);

    auto *make_aggregate_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_A)});
    if (make_aggregate_shortcut != nullptr) {
        connect(make_aggregate_shortcut, &QAction::triggered, this, &W3DViewMainWindow::makeAggregate);
    }
    auto *advanced_animation_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_V)});
    if (advanced_animation_shortcut != nullptr) {
        connect(advanced_animation_shortcut, &QAction::triggered,
                this, &W3DViewMainWindow::openAdvancedAnimation);
    }
    auto *lod_record_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::Key_Space)});
    if (lod_record_shortcut != nullptr) {
        connect(lod_record_shortcut, &QAction::triggered, this, &W3DViewMainWindow::recordLodScreenArea);
    }
    auto *lod_prev_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::Key_BracketLeft)});
    if (lod_prev_shortcut != nullptr) {
        connect(lod_prev_shortcut, &QAction::triggered, this, &W3DViewMainWindow::selectPrevLod);
    }
    auto *lod_next_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::Key_BracketRight)});
    if (lod_next_shortcut != nullptr) {
        connect(lod_next_shortcut, &QAction::triggered, this, &W3DViewMainWindow::selectNextLod);
    }
    auto *object_rotate_y_back_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::Key_Down)});
    if (object_rotate_y_back_shortcut != nullptr) {
        connect(object_rotate_y_back_shortcut, &QAction::triggered,
                this, &W3DViewMainWindow::toggleObjectRotateYBack);
    }
    auto *object_rotate_z_back_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::Key_Left)});
    if (object_rotate_z_back_shortcut != nullptr) {
        connect(object_rotate_z_back_shortcut, &QAction::triggered,
                this, &W3DViewMainWindow::toggleObjectRotateZBack);
    }
    auto *light_rotate_y_back_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_Down)});
    if (light_rotate_y_back_shortcut != nullptr) {
        connect(light_rotate_y_back_shortcut, &QAction::triggered,
                this, &W3DViewMainWindow::toggleLightRotateYBack);
    }
    auto *light_rotate_z_back_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_Left)});
    if (light_rotate_z_back_shortcut != nullptr) {
        connect(light_rotate_z_back_shortcut, &QAction::triggered,
                this, &W3DViewMainWindow::toggleLightRotateZBack);
    }
    for (int slot = 1; slot <= 9; ++slot) {
        const Qt::Key key = static_cast<Qt::Key>(Qt::Key_1 + (slot - 1));
        auto *settings_shortcut =
            CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(key)});
        if (settings_shortcut != nullptr) {
            connect(settings_shortcut, &QAction::triggered, this, [this, slot]() {
                loadQuickSettings(slot);
            });
        }
    }
    auto *next_pane_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::Key_F6)});
    if (next_pane_shortcut != nullptr) {
        connect(next_pane_shortcut, &QAction::triggered, this, [this]() { cyclePaneFocus(false); });
    }
    auto *prev_pane_shortcut =
        CreateWindowShortcutAction(this, QList<QKeySequence>{QKeySequence(Qt::SHIFT | Qt::Key_F6)});
    if (prev_pane_shortcut != nullptr) {
        connect(prev_pane_shortcut, &QAction::triggered, this, [this]() { cyclePaneFocus(true); });
    }

    _listMissingTexturesAction = new QAction("List Missing Textures", this);
    connect(_listMissingTexturesAction, &QAction::triggered, this,
            &W3DViewMainWindow::listMissingTextures);
    _copyAssetsAction = new QAction("Copy Asset Files...", this);
    connect(_copyAssetsAction, &QAction::triggered, this, &W3DViewMainWindow::copyAssets);
    _copyAssetsAction->setEnabled(false);
    _addToLineupAction = new QAction("Add To Lineup...", this);
    connect(_addToLineupAction, &QAction::triggered, this, &W3DViewMainWindow::addToLineup);
    _addToLineupAction->setEnabled(false);

    _mainToolbar = addToolBar("Main");
    _mainToolbar->setObjectName("MainToolbar");
    _mainToolbar->addAction(_newAction);
    _mainToolbar->addAction(_openAction);
    _mainToolbar->addSeparator();
    _mainToolbar->addAction(_exportEmitterAction);
    _mainToolbar->addAction(_exportAggregateAction);
    _mainToolbar->addAction(_exportLodAction);
    _mainToolbar->addAction(_exportPrimitiveAction);
    _mainToolbar->addAction(_exportSoundObjectAction);
    _mainToolbar->addSeparator();
    _mainToolbar->addAction(_listMissingTexturesAction);
    _mainToolbar->addSeparator();
    _mainToolbar->addAction(_copyAssetsAction);
    _mainToolbar->addSeparator();
    _mainToolbar->addAction(_addToLineupAction);
    _mainToolbar->addSeparator();
    _mainToolbar->addAction(_aboutAction);

    _objectToolbar = addToolBar("Object");
    _objectToolbar->setObjectName("ObjectToolbar");
    _objectToolbar->addAction(_cameraRotateXAction);
    _objectToolbar->addAction(_cameraRotateYAction);
    _objectToolbar->addAction(_cameraRotateZAction);
    _objectToolbar->addAction(_objectRotateZAction);

    _animationToolbar = addToolBar("Animation");
    _animationToolbar->setObjectName("AnimationToolbar");
    auto *anim_play_action = _animationToolbar->addAction("Play");
    connect(anim_play_action, &QAction::triggered, this, &W3DViewMainWindow::startAnimation);
    auto *anim_stop_action = _animationToolbar->addAction("Stop");
    connect(anim_stop_action, &QAction::triggered, this, &W3DViewMainWindow::stopAnimation);
    auto *anim_pause_action = _animationToolbar->addAction("Pause");
    connect(anim_pause_action, &QAction::triggered, this, &W3DViewMainWindow::pauseAnimation);
    auto *anim_step_back_action = _animationToolbar->addAction("Step Back");
    connect(anim_step_back_action, &QAction::triggered, this,
            &W3DViewMainWindow::stepAnimationBackward);
    auto *anim_step_forward_action = _animationToolbar->addAction("Step Forward");
    connect(anim_step_forward_action, &QAction::triggered, this,
            &W3DViewMainWindow::stepAnimationForward);

    if (_animationToolbar) {
        _animationToolbar->setVisible(false);
    }
    if (_toolbarAnimationAction) {
        _toolbarAnimationAction->setChecked(false);
    }

    statusBar()->showMessage("Ready");

    auto *splitter = new QSplitter(this);
    splitter->setChildrenCollapsible(false);

    _treeView = new QTreeView(splitter);
    _treeModel = new QStandardItemModel(_treeView);
    _treeModel->setHorizontalHeaderLabels(QStringList() << "Assets");
    _treeView->setModel(_treeModel);
    _treeView->setHeaderHidden(false);
    _treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(_treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &W3DViewMainWindow::onCurrentChanged);
    connect(_treeView, &QTreeView::customContextMenuRequested,
            this, &W3DViewMainWindow::showTreeContextMenu);

    _viewport = new W3DViewport(splitter);

    _statusPolysLabel = new QLabel(this);
    _statusParticlesLabel = new QLabel(this);
    _statusCameraLabel = new QLabel(this);
    _statusFramesLabel = new QLabel(this);
    _statusFpsLabel = new QLabel(this);
    _statusResolutionLabel = new QLabel(this);

    if (statusBar()) {
        statusBar()->addPermanentWidget(_statusPolysLabel);
        statusBar()->addPermanentWidget(_statusParticlesLabel);
        statusBar()->addPermanentWidget(_statusCameraLabel);
        statusBar()->addPermanentWidget(_statusFramesLabel, 1);
        statusBar()->addPermanentWidget(_statusFpsLabel);
        statusBar()->addPermanentWidget(_statusResolutionLabel);
    }

    _statusTimer = new QTimer(this);
    _statusTimer->setInterval(250);
    connect(_statusTimer, &QTimer::timeout, this, &W3DViewMainWindow::updateStatusBar);
    _statusTimer->start();

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 800});

    setCentralWidget(splitter);
    resize(1200, 800);

    loadAppSettings();
    loadDefaultSettings();
    if (_restrictAnimsAction) {
        _restrictAnimsAction->setChecked(_restrictAnims);
    }
    if (_sortingAction) {
        _sortingAction->setChecked(_sortingEnabled);
    }
    if (_invertBackfaceCullingAction) {
        _invertBackfaceCullingAction->setChecked(ShaderClass::Is_Backface_Culling_Inverted());
    }
    if (_wireframeAction && _viewport) {
        _wireframeAction->setChecked(_viewport->isWireframeEnabled());
    }
    if (_statusBarAction) {
        _statusBarAction->setChecked(statusBar()->isVisible());
    }
    if (_fogAction && _viewport) {
        _fogAction->setChecked(_viewport->isFogEnabled());
    }
    if (_cameraResetOnDisplayAction) {
        _cameraResetOnDisplayAction->setChecked(_autoResetCamera);
    }
    if (_cameraAnimateAction) {
        _cameraAnimateAction->setChecked(_animateCamera);
    }
    if (_cameraBonePosXAction && _viewport) {
        _cameraBonePosXAction->setChecked(_viewport->isCameraBonePosX());
    }
    if (_exposePrelitAction) {
        _exposePrelitAction->setChecked(WW3D::Expose_Prelit());
    }
    if (_prelitGroup) {
        const WW3D::PrelitModeEnum mode = WW3D::Get_Prelit_Mode();
        if (_prelitVertexAction && mode == WW3D::PRELIT_MODE_VERTEX) {
            _prelitVertexAction->setChecked(true);
        } else if (_prelitMultipassAction && mode == WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS) {
            _prelitMultipassAction->setChecked(true);
        } else if (_prelitMultitexAction && mode == WW3D::PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE) {
            _prelitMultitexAction->setChecked(true);
        }
    }
    rebuildAssetTree();
}

void W3DViewMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event) {
        return;
    }

    const QMimeData *mime = event->mimeData();
    if (!mime || !mime->hasUrls()) {
        return;
    }

    const auto urls = mime->urls();
    for (const auto &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (path.endsWith(".w3d", Qt::CaseInsensitive)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void W3DViewMainWindow::dropEvent(QDropEvent *event)
{
    if (!event) {
        return;
    }

    const QMimeData *mime = event->mimeData();
    if (!mime || !mime->hasUrls()) {
        return;
    }

    bool loaded_any = false;
    const auto urls = mime->urls();
    for (const auto &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (!path.endsWith(".w3d", Qt::CaseInsensitive)) {
            continue;
        }
        if (loadAssetsFromFile(path)) {
            loaded_any = true;
        }
    }

    if (loaded_any) {
        event->acceptProposedAction();
    }
}

void W3DViewMainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open W3D Asset",
        _lastOpenedPath,
        "W3D Assets (*.w3d);;All Files (*.*)");

    if (path.isEmpty()) {
        return;
    }

    loadAssetsFromFile(path);
}

void W3DViewMainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action) {
        return;
    }

    const QString path = action->data().toString();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "W3DViewQt", QString("File not found:\n%1").arg(path));
        QSettings settings;
        auto files = settings.value(QStringLiteral("recentFiles")).toStringList();
        files.removeAll(path);
        settings.setValue(QStringLiteral("recentFiles"), files);
        updateRecentFilesMenu();
        return;
    }

    if (!loadAssetsFromFile(path)) {
        QSettings settings;
        auto files = settings.value(QStringLiteral("recentFiles")).toStringList();
        files.removeAll(path);
        settings.setValue(QStringLiteral("recentFiles"), files);
        updateRecentFilesMenu();
    }
}

void W3DViewMainWindow::newFile()
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (asset_manager) {
        asset_manager->Free_Assets();
    }

    if (_viewport) {
        _viewport->setRenderObject(nullptr);
        _viewport->clearAnimation();
    }

    _lastOpenedPath.clear();
    setWindowTitle("W3DViewQt");
    rebuildAssetTree();
    statusBar()->showMessage("Cleared assets.");
}

void W3DViewMainWindow::openTexturePathsDialog()
{
    TexturePathDialog dialog(_texturePath1, _texturePath2, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    setTexturePaths(dialog.path1(), dialog.path2());
}

void W3DViewMainWindow::loadSettingsFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Load Settings",
        _lastOpenedPath,
        "W3D Settings (*.dat *.ini);;All Files (*.*)");

    if (path.isEmpty() || !_viewport) {
        return;
    }

    QSettings settings(path, QSettings::IniFormat);
    applySettings(settings);
    statusBar()->showMessage(QString("Loaded settings: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::saveSettingsFile()
{
    if (!_viewport) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save Settings",
        _lastOpenedPath,
        "W3D Settings (*.dat *.ini);;All Files (*.*)");

    if (path.isEmpty()) {
        return;
    }

    QSettings settings(path, QSettings::IniFormat);
    writeSettings(settings);
    settings.sync();

    statusBar()->showMessage(QString("Saved settings: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::loadQuickSettings(int slot)
{
    if (slot < 1 || slot > 9 || !_viewport) {
        return;
    }

    const QString path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QString("settings%1.dat").arg(slot));
    if (!QFileInfo::exists(path)) {
        return;
    }

    QSettings settings(path, QSettings::IniFormat);
    applySettings(settings);
}

void W3DViewMainWindow::cyclePaneFocus(bool reverse)
{
    if (!_treeView || !_viewport) {
        return;
    }

    QWidget *focused = QApplication::focusWidget();
    const bool tree_has_focus = focused == _treeView || _treeView->isAncestorOf(focused);
    const bool viewport_has_focus = focused == _viewport || _viewport->isAncestorOf(focused);
    if (tree_has_focus) {
        _viewport->setFocus();
        return;
    }
    if (viewport_has_focus) {
        _treeView->setFocus();
        return;
    }

    if (reverse) {
        _viewport->setFocus();
    } else {
        _treeView->setFocus();
    }
}

void W3DViewMainWindow::onCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    updateSpecialMenu(current);
    if (!_viewport) {
        return;
    }

    const int type_value = current.data(kRoleType).toInt();
    const bool is_render_object = type_value == static_cast<int>(AssetNodeType::RenderObject);
    const int class_id = current.data(kRoleClassId).toInt();
    const bool is_sound = is_render_object && class_id == RenderObjClass::CLASSID_SOUND;
    const bool is_emitter = is_render_object && class_id == RenderObjClass::CLASSID_PARTICLEEMITTER;
    const bool is_primitive = is_render_object &&
        (class_id == RenderObjClass::CLASSID_SPHERE || class_id == RenderObjClass::CLASSID_RING);
    const bool is_animation = type_value == static_cast<int>(AssetNodeType::Animation);
    bool can_lineup = false;
    if (is_render_object && _viewport) {
        can_lineup = _viewport->canLineUpClass(class_id);
    }
    if (_copyAssetsAction) {
        _copyAssetsAction->setEnabled(is_render_object);
    }
    if (_addToLineupAction) {
        _addToLineupAction->setEnabled(can_lineup);
    }
    if (_editSoundObjectAction) {
        _editSoundObjectAction->setEnabled(is_sound);
    }
    if (_exportSoundObjectAction) {
        _exportSoundObjectAction->setEnabled(is_sound);
    }
    if (_exportEmitterAction) {
        _exportEmitterAction->setEnabled(is_emitter);
    }
    if (_exportPrimitiveAction) {
        _exportPrimitiveAction->setEnabled(is_primitive);
    }
    if (_editEmitterAction) {
        _editEmitterAction->setEnabled(is_emitter);
    }
    if (_scaleEmitterAction) {
        _scaleEmitterAction->setEnabled(is_emitter);
    }
    if (_editPrimitiveAction) {
        _editPrimitiveAction->setEnabled(is_primitive);
    }
    if (_objectPropertiesAction) {
        _objectPropertiesAction->setEnabled(is_render_object || is_animation);
    }
    if (_makeMovieAction) {
        _makeMovieAction->setEnabled(is_animation);
    }
    if (type_value == static_cast<int>(AssetNodeType::RenderObject)) {
        _viewport->clearAnimation();

        const QString name = current.data(kRoleName).toString();
        if (name.isEmpty()) {
            return;
        }

        auto *asset_manager = WW3DAssetManager::Get_Instance();
        if (!asset_manager) {
            return;
        }

        const QByteArray name_bytes = name.toLatin1();
        RenderObjClass *object = asset_manager->Create_Render_Obj(name_bytes.constData());
        if (!object) {
            statusBar()->showMessage(QString("Failed to create render object: %1").arg(name));
            return;
        }

        SetHighestLod(object);
        _viewport->setRenderObject(object);
        object->Release_Ref();
        statusBar()->showMessage(QString("Showing: %1").arg(name));
        updateEmittersEditMenu();
        return;
    }

    if (type_value == static_cast<int>(AssetNodeType::Animation)) {
        auto *asset_manager = WW3DAssetManager::Get_Instance();
        if (!asset_manager) {
            return;
        }

        const QString animation_name = current.data(kRoleName).toString();
        if (animation_name.isEmpty()) {
            return;
        }

        QModelIndex render_index = current.parent();
        while (render_index.isValid() &&
               render_index.data(kRoleType).toInt() != static_cast<int>(AssetNodeType::RenderObject)) {
            render_index = render_index.parent();
        }

        if (!render_index.isValid()) {
            return;
        }

        const QString render_name = render_index.data(kRoleName).toString();
        if (render_name.isEmpty()) {
            return;
        }

        const QByteArray render_bytes = render_name.toLatin1();
        RenderObjClass *object = asset_manager->Create_Render_Obj(render_bytes.constData());
        if (!object) {
            statusBar()->showMessage(QString("Failed to create render object: %1").arg(render_name));
            return;
        }

        const QByteArray anim_bytes = animation_name.toLatin1();
        HAnimClass *animation = asset_manager->Get_HAnim(anim_bytes.constData());
        if (!animation) {
            object->Release_Ref();
            statusBar()->showMessage(QString("Failed to load animation: %1").arg(animation_name));
            return;
        }

        SetHighestLod(object);
        _viewport->setRenderObject(object);
        _viewport->setAnimation(animation);
        object->Release_Ref();
        animation->Release_Ref();
        statusBar()->showMessage(
            QString("Playing: %1 (%2)").arg(animation_name, render_name));
        updateEmittersEditMenu();
        return;
    }

    if (type_value == static_cast<int>(AssetNodeType::Material)) {
        _viewport->clearAnimation();

        const QString name = current.data(kRoleName).toString();
        const quintptr texture_ptr = current.data(kRolePointer).value<quintptr>();
        auto *texture = reinterpret_cast<TextureClass *>(texture_ptr);
        if (!texture) {
            statusBar()->showMessage(QString("Missing texture: %1").arg(name));
            return;
        }

        auto *bitmap = new Bitmap2DObjClass(texture, 0.5f, 0.5f, true, false, false, true);
        Matrix3D transform(1);
        transform.Rotate_Y(static_cast<float>(kPi * 0.5));
        bitmap->Set_Transform(transform);
        _viewport->setRenderObject(bitmap);
        bitmap->Release_Ref();
        statusBar()->showMessage(QString("Showing material: %1").arg(name));
        updateEmittersEditMenu();
        return;
    }
}

void W3DViewMainWindow::updateSpecialMenu(const QModelIndex &current)
{
    if (!_objectMenuAction || !menuBar()) {
        return;
    }

    QMenu *desired_menu = nullptr;
    const int type_value = current.data(kRoleType).toInt();
    const bool is_group = type_value == static_cast<int>(AssetNodeType::Group);

    auto matches_group = [](const QString &text, const QString &label) {
        return text == label || text.startsWith(label + " (");
    };

    if (type_value == static_cast<int>(AssetNodeType::Animation)) {
        desired_menu = _animationMenu;
    } else if (type_value == static_cast<int>(AssetNodeType::RenderObject) || is_group) {
        const QString group_label = ResolveGroupLabel(_treeModel, current, is_group);
        if (matches_group(group_label, "H-LOD")) {
            desired_menu = _lodMenu;
        } else if (matches_group(group_label, "Hierarchy")) {
            desired_menu = _hierarchyMenu;
        } else if (matches_group(group_label, "Aggregate")) {
            desired_menu = _aggregateMenu;
        }
    }

    QAction *desired_action = desired_menu ? desired_menu->menuAction() : nullptr;
    if (_specialMenuAction && _specialMenuAction != desired_action) {
        menuBar()->removeAction(_specialMenuAction);
    }
    if (desired_action && !menuBar()->actions().contains(desired_action)) {
        menuBar()->insertMenu(_objectMenuAction, desired_menu);
    }
    _specialMenuAction = desired_action;
}

void W3DViewMainWindow::updateEmittersEditMenu()
{
    if (!_emittersEditMenu) {
        return;
    }

    _emittersEditMenu->clear();

    QStringList names;
    if (_viewport) {
        if (auto *render_obj = _viewport->currentRenderObject()) {
            CollectEmitterNames(*render_obj, names);
        }
    }

    if (names.isEmpty()) {
        auto *empty_action = _emittersEditMenu->addAction("(No Emitters)");
        empty_action->setEnabled(false);
        return;
    }

    names.sort(Qt::CaseInsensitive);
    for (const auto &name : names) {
        auto *action = _emittersEditMenu->addAction(name);
        connect(action, &QAction::triggered, this, [this, name]() { editEmitterByName(name); });
    }
}

void W3DViewMainWindow::refreshAnimationMenu()
{
    const bool has_anim = _viewport && _viewport->hasAnimation();
    if (_animationPlayAction) {
        _animationPlayAction->setEnabled(has_anim);
    }
    if (_animationPauseAction) {
        _animationPauseAction->setEnabled(has_anim);
    }
    if (_animationStopAction) {
        _animationStopAction->setEnabled(has_anim);
    }
    if (_animationStepBackAction) {
        _animationStepBackAction->setEnabled(has_anim);
    }
    if (_animationStepForwardAction) {
        _animationStepForwardAction->setEnabled(has_anim);
    }
}

void W3DViewMainWindow::refreshAggregateMenu()
{
    if (!_aggregateBindSubobjectAction) {
        return;
    }
    const bool bound = _viewport && _viewport->isSubobjectLodBound();
    _aggregateBindSubobjectAction->setChecked(bound);
}

void W3DViewMainWindow::refreshLodMenu()
{
    if (!_viewport) {
        if (_lodPrevAction) {
            _lodPrevAction->setEnabled(false);
        }
        if (_lodNextAction) {
            _lodNextAction->setEnabled(false);
        }
        return;
    }

    if (_lodIncludeNullAction) {
        _lodIncludeNullAction->setChecked(_viewport->isNullLodIncluded());
    }
    if (_lodAutoSwitchAction) {
        _lodAutoSwitchAction->setChecked(_viewport->isLodAutoSwitchingEnabled());
    }

    int level = 0;
    int count = 0;
    const bool has_lod = _viewport->currentLodInfo(level, count);
    if (_lodPrevAction) {
        _lodPrevAction->setEnabled(has_lod && level > 0);
    }
    if (_lodNextAction) {
        _lodNextAction->setEnabled(has_lod && (level + 1) < count);
    }
}

void W3DViewMainWindow::editEmitterByName(const QString &name)
{
    if (name.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Edit Emitter", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to load emitter.");
        return;
    }

    if (render_obj->Class_ID() != RenderObjClass::CLASSID_PARTICLEEMITTER) {
        render_obj->Release_Ref();
        QMessageBox::warning(this, "Edit Emitter", "Selected object is not an emitter.");
        return;
    }

    auto *emitter = static_cast<ParticleEmitterClass *>(render_obj);
    ParticleEmitterDefClass *definition = emitter->Build_Definition();
    emitter->Release_Ref();
    if (!definition) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to load emitter definition.");
        return;
    }

    EmitterEditDialog dialog(*definition, this);
    delete definition;
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::unique_ptr<ParticleEmitterDefClass> updated(dialog.definition());
    if (!updated) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to update emitter definition.");
        return;
    }

    if (!UpdateEmitterPrototype(*updated, dialog.originalName())) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to register emitter prototype.");
        return;
    }

    reloadDisplayedObject();
    statusBar()->showMessage(QString("Updated emitter: %1").arg(name));
}

void W3DViewMainWindow::updateStatusBar()
{
    if (!_viewport) {
        return;
    }

    int polys = 0;
    int particles = 0;
    if (auto *render_obj = _viewport->currentRenderObject()) {
        polys = render_obj->Get_Num_Polys();
        particles = CountParticles(render_obj);
    }

    if (_statusPolysLabel) {
        _statusPolysLabel->setText(QString("Polys %1").arg(polys));
    }
    if (_statusParticlesLabel) {
        _statusParticlesLabel->setText(QString("Particles %1").arg(particles));
    }
    if (_statusCameraLabel) {
        _statusCameraLabel->setText(QString("Camera %1").arg(_viewport->cameraDistance(), 0, 'f', 3));
    }

    int current_frame = 0;
    int total_frames = 0;
    float fps = 0.0f;
    const bool has_anim = _viewport->animationStatus(current_frame, total_frames, fps);
    if (_statusFramesLabel) {
        const int max_frame = total_frames > 0 ? total_frames - 1 : 0;
        const int display_frame = has_anim ? current_frame : 0;
        const float display_fps = has_anim ? fps : 0.0f;
        _statusFramesLabel->setText(
            QString("Frame %1/%2 at %3 fps")
                .arg(display_frame)
                .arg(max_frame)
                .arg(display_fps, 0, 'f', 2));
    }

    if (_statusFpsLabel) {
        const float frame_ms = _viewport->averageFrameMilliseconds();
        if (frame_ms > 0.0f) {
            _statusFpsLabel->setText(QString("Clocks: %1").arg(frame_ms, 0, 'f', 2));
        } else {
            _statusFpsLabel->setText(QString());
        }
    }

    if (_statusResolutionLabel) {
        _statusResolutionLabel->setText(QString(" %1 x %2 ").arg(_viewport->width()).arg(_viewport->height()));
    }
}

void W3DViewMainWindow::toggleWireframe(bool enabled)
{
    if (_viewport) {
        _viewport->setWireframeEnabled(enabled);
    }
}

void W3DViewMainWindow::toggleSorting(bool enabled)
{
    WW3D::_Invalidate_Mesh_Cache();
    WW3D::Enable_Sorting(enabled);
    _sortingEnabled = enabled;

    QSettings settings;
    settings.setValue("Config/EnableSorting", enabled);
}

void W3DViewMainWindow::toggleAutoExpandAssetTree(bool enabled)
{
    _autoExpandAssetTree = enabled;

    QSettings settings;
    settings.setValue("Config/AutoExpandAssetTree", enabled);

    if (!_treeView || !_treeModel) {
        return;
    }

    auto *root = _treeModel->invisibleRootItem();
    if (!root) {
        return;
    }

    const int count = root->rowCount();
    for (int index = 0; index < count; ++index) {
        auto *item = root->child(index);
        if (!item) {
            continue;
        }
        _treeView->setExpanded(item->index(), enabled);
    }
}

void W3DViewMainWindow::toggleBackfaceCulling(bool inverted)
{
    ShaderClass::Invert_Backface_Culling(inverted);
    QSettings settings;
    settings.setValue("Config/InvertBackfaceCulling", inverted);
}

void W3DViewMainWindow::toggleRestrictAnims(bool enabled)
{
    if (_restrictAnims == enabled) {
        return;
    }

    _restrictAnims = enabled;
    rebuildAssetTree();
}

void W3DViewMainWindow::toggleStatusBar(bool visible)
{
    if (statusBar()) {
        statusBar()->setVisible(visible);
    }
}

void W3DViewMainWindow::toggleMainToolbar(bool visible)
{
    if (_mainToolbar) {
        _mainToolbar->setVisible(visible);
    }
}

void W3DViewMainWindow::toggleObjectToolbar(bool visible)
{
    if (_objectToolbar) {
        _objectToolbar->setVisible(visible);
    }
}

void W3DViewMainWindow::toggleAnimationToolbar(bool visible)
{
    if (_animationToolbar) {
        _animationToolbar->setVisible(visible);
    }
}

void W3DViewMainWindow::setAmbientLight()
{
    if (!_viewport) {
        return;
    }

    const QColor current = ToQColor(_viewport->ambientLight());
    const QColor selected = QColorDialog::getColor(current, this, "Ambient Light");
    if (!selected.isValid()) {
        return;
    }

    _viewport->setAmbientLight(ToVector3(selected));
}

void W3DViewMainWindow::setSceneLight()
{
    if (!_viewport) {
        return;
    }

    const QColor current = ToQColor(_viewport->sceneLightColor());
    const QColor selected = QColorDialog::getColor(current, this, "Scene Light");
    if (!selected.isValid()) {
        return;
    }

    _viewport->setSceneLightColor(ToVector3(selected));
}

void W3DViewMainWindow::increaseAmbientLight()
{
    if (!_viewport) {
        return;
    }

    Vector3 color = _viewport->ambientLight();
    AdjustLightIntensity(color, 0.05f);
    _viewport->setAmbientLight(color);
}

void W3DViewMainWindow::decreaseAmbientLight()
{
    if (!_viewport) {
        return;
    }

    Vector3 color = _viewport->ambientLight();
    AdjustLightIntensity(color, -0.05f);
    _viewport->setAmbientLight(color);
}

void W3DViewMainWindow::increaseSceneLight()
{
    if (!_viewport) {
        return;
    }

    Vector3 color = _viewport->sceneLightColor();
    AdjustLightIntensity(color, 0.05f);
    _viewport->setSceneLightColor(color);
}

void W3DViewMainWindow::decreaseSceneLight()
{
    if (!_viewport) {
        return;
    }

    Vector3 color = _viewport->sceneLightColor();
    AdjustLightIntensity(color, -0.05f);
    _viewport->setSceneLightColor(color);
}

void W3DViewMainWindow::killSceneLight()
{
    if (!_viewport) {
        return;
    }

    _viewport->setSceneLightColor(Vector3(0.0f, 0.0f, 0.0f));
}

void W3DViewMainWindow::toggleLightRotateY(bool enabled)
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->lightRotationFlags();
    if (enabled) {
        flags |= W3DViewport::RotateY;
        flags &= ~W3DViewport::RotateYBack;
    } else {
        flags &= ~W3DViewport::RotateY;
    }
    _viewport->setLightRotationFlags(flags);
}

void W3DViewMainWindow::toggleLightRotateYBack()
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->lightRotationFlags();
    flags ^= W3DViewport::RotateYBack;
    flags &= ~W3DViewport::RotateY;
    _viewport->setLightRotationFlags(flags);
    if (_lightRotateYAction) {
        const QSignalBlocker blocker(_lightRotateYAction);
        _lightRotateYAction->setChecked((flags & W3DViewport::RotateY) != 0);
    }
}

void W3DViewMainWindow::toggleLightRotateZ(bool enabled)
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->lightRotationFlags();
    if (enabled) {
        flags |= W3DViewport::RotateZ;
        flags &= ~W3DViewport::RotateZBack;
    } else {
        flags &= ~W3DViewport::RotateZ;
    }
    _viewport->setLightRotationFlags(flags);
}

void W3DViewMainWindow::toggleLightRotateZBack()
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->lightRotationFlags();
    flags ^= W3DViewport::RotateZBack;
    flags &= ~W3DViewport::RotateZ;
    _viewport->setLightRotationFlags(flags);
    if (_lightRotateZAction) {
        const QSignalBlocker blocker(_lightRotateZAction);
        _lightRotateZAction->setChecked((flags & W3DViewport::RotateZ) != 0);
    }
}

void W3DViewMainWindow::toggleExposePrelit(bool enabled)
{
    WW3D::Expose_Prelit(enabled);
}

void W3DViewMainWindow::setPrelitVertex()
{
    if (WW3D::Get_Prelit_Mode() == WW3D::PRELIT_MODE_VERTEX) {
        return;
    }

    WW3D::Set_Prelit_Mode(WW3D::PRELIT_MODE_VERTEX);
    reloadLightmapModels();
    reloadDisplayedObject();
}

void W3DViewMainWindow::setPrelitMultipass()
{
    if (WW3D::Get_Prelit_Mode() == WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS) {
        return;
    }

    WW3D::Set_Prelit_Mode(WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS);
    reloadLightmapModels();
    reloadDisplayedObject();
}

void W3DViewMainWindow::setPrelitMultitex()
{
    if (WW3D::Get_Prelit_Mode() == WW3D::PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE) {
        return;
    }

    WW3D::Set_Prelit_Mode(WW3D::PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE);
    reloadLightmapModels();
    reloadDisplayedObject();
}

void W3DViewMainWindow::setBackgroundColor()
{
    if (!_viewport) {
        return;
    }

    const QColor current = ToQColor(_viewport->backgroundColor());
    const QColor selected = QColorDialog::getColor(current, this, "Background Color");
    if (!selected.isValid()) {
        return;
    }

    _viewport->setBackgroundColor(ToVector3(selected));
}

void W3DViewMainWindow::setBackgroundBitmap()
{
    if (!_viewport) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        "Background Bitmap",
        _lastOpenedPath,
        "Images (*.bmp *.tga *.dds);;All Files (*.*)");

    if (path.isEmpty()) {
        return;
    }

    _viewport->setBackgroundBitmap(path);
    statusBar()->showMessage(QString("Background bitmap: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::toggleFog(bool enabled)
{
    if (_viewport) {
        _viewport->setFogEnabled(enabled);
    }
}

void W3DViewMainWindow::setCameraFront()
{
    if (_viewport) {
        _viewport->setCameraPosition(W3DViewport::CameraPosition::Front);
    }
}

void W3DViewMainWindow::setCameraBack()
{
    if (_viewport) {
        _viewport->setCameraPosition(W3DViewport::CameraPosition::Back);
    }
}

void W3DViewMainWindow::setCameraLeft()
{
    if (_viewport) {
        _viewport->setCameraPosition(W3DViewport::CameraPosition::Left);
    }
}

void W3DViewMainWindow::setCameraRight()
{
    if (_viewport) {
        _viewport->setCameraPosition(W3DViewport::CameraPosition::Right);
    }
}

void W3DViewMainWindow::setCameraTop()
{
    if (_viewport) {
        _viewport->setCameraPosition(W3DViewport::CameraPosition::Top);
    }
}

void W3DViewMainWindow::setCameraBottom()
{
    if (_viewport) {
        _viewport->setCameraPosition(W3DViewport::CameraPosition::Bottom);
    }
}

void W3DViewMainWindow::resetCamera()
{
    if (_viewport) {
        _viewport->resetCamera();
    }
}

void W3DViewMainWindow::setCameraRotateX(bool enabled)
{
    if (!_viewport) {
        return;
    }

    if (enabled) {
        _viewport->setAllowedCameraRotation(W3DViewport::CameraRotation::OnlyX);
        if (_cameraRotateYAction) {
            _cameraRotateYAction->setChecked(false);
        }
        if (_cameraRotateZAction) {
            _cameraRotateZAction->setChecked(false);
        }
    } else if (_viewport->allowedCameraRotation() == W3DViewport::CameraRotation::OnlyX) {
        _viewport->setAllowedCameraRotation(W3DViewport::CameraRotation::Free);
    }
}

void W3DViewMainWindow::setCameraRotateY(bool enabled)
{
    if (!_viewport) {
        return;
    }

    if (enabled) {
        _viewport->setAllowedCameraRotation(W3DViewport::CameraRotation::OnlyY);
        if (_cameraRotateXAction) {
            _cameraRotateXAction->setChecked(false);
        }
        if (_cameraRotateZAction) {
            _cameraRotateZAction->setChecked(false);
        }
    } else if (_viewport->allowedCameraRotation() == W3DViewport::CameraRotation::OnlyY) {
        _viewport->setAllowedCameraRotation(W3DViewport::CameraRotation::Free);
    }
}

void W3DViewMainWindow::setCameraRotateZ(bool enabled)
{
    if (!_viewport) {
        return;
    }

    if (enabled) {
        _viewport->setAllowedCameraRotation(W3DViewport::CameraRotation::OnlyZ);
        if (_cameraRotateXAction) {
            _cameraRotateXAction->setChecked(false);
        }
        if (_cameraRotateYAction) {
            _cameraRotateYAction->setChecked(false);
        }
    } else if (_viewport->allowedCameraRotation() == W3DViewport::CameraRotation::OnlyZ) {
        _viewport->setAllowedCameraRotation(W3DViewport::CameraRotation::Free);
    }
}

void W3DViewMainWindow::toggleCameraAnimate(bool enabled)
{
    _animateCamera = enabled;
    if (_viewport) {
        _viewport->setCameraAnimationEnabled(enabled);
    }

    QSettings settings;
    settings.setValue("Config/AnimateCamera", enabled);
}

void W3DViewMainWindow::toggleCameraResetOnDisplay(bool enabled)
{
    _autoResetCamera = enabled;
    if (_viewport) {
        _viewport->setAutoResetEnabled(enabled);
    }

    QSettings settings;
    settings.setValue("Config/ResetCamera", enabled);
}

void W3DViewMainWindow::toggleCameraBonePosX(bool enabled)
{
    if (_viewport) {
        _viewport->setCameraBonePosX(enabled);
    }
}

void W3DViewMainWindow::openCameraSettings()
{
    if (!_viewport) {
        return;
    }

    CameraSettingsDialog dialog(_viewport, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const bool manual_fov = dialog.isManualFovEnabled();
    const bool manual_clip = dialog.isManualClipPlanesEnabled();

    if (manual_fov) {
        _viewport->setManualFovEnabled(true);
        _viewport->setCameraFovDegrees(dialog.hfovDegrees(), dialog.vfovDegrees());
    } else {
        _viewport->setManualFovEnabled(false);
        _viewport->resetFov();
    }

    _viewport->setManualClipPlanesEnabled(manual_clip);
    _viewport->setCameraClipPlanes(dialog.nearClip(), dialog.farClip());

    double hfov_deg = 0.0;
    double vfov_deg = 0.0;
    _viewport->cameraFovDegrees(hfov_deg, vfov_deg);
    float znear = 0.0f;
    float zfar = 0.0f;
    _viewport->cameraClipPlanes(znear, zfar);

    QSettings settings;
    settings.setValue("Config/UseManualFOV", manual_fov);
    settings.setValue("Config/UseManualClipPlanes", manual_clip);
    settings.setValue("Config/hfov", hfov_deg * kDegToRad);
    settings.setValue("Config/vfov", vfov_deg * kDegToRad);
    settings.setValue("Config/znear", znear);
    settings.setValue("Config/zfar", zfar);
}

void W3DViewMainWindow::openCameraDistance()
{
    if (!_viewport) {
        return;
    }

    CameraDistanceDialog dialog(_viewport->cameraDistance(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    _viewport->setCameraDistance(dialog.distance());
}

void W3DViewMainWindow::copyScreenSize()
{
    if (!_viewport) {
        return;
    }

    const float size = _viewport->currentScreenSize();
    if (size <= 0.0f) {
        statusBar()->showMessage("No render object to measure.");
        return;
    }

    const QString text = QString("MaxScreenSize=%1").arg(size, 0, 'f', 6);
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
        statusBar()->showMessage("Copied screen size to clipboard.");
    }
}

void W3DViewMainWindow::openGammaDialog()
{
    if (_enableGammaAction && !_enableGammaAction->isChecked()) {
        QMessageBox::warning(this, "Gamma", "Gamma is disabled.\nEnable it in the File menu.");
        return;
    }

    GammaDialog dialog(this);
    dialog.exec();
}

void W3DViewMainWindow::toggleGammaCorrection(bool enabled)
{
    QSettings settings;
    settings.setValue("Config/EnableGamma", enabled ? 1 : 0);

    if (enabled) {
        int gamma = settings.value("Config/Gamma", 10).toInt();
        if (gamma < 10) {
            gamma = 10;
        }
        if (gamma > 30) {
            gamma = 30;
        }
        DX8Wrapper::Set_Gamma(gamma / 10.0f, 0.0f, 1.0f);
    } else {
        DX8Wrapper::Set_Gamma(1.0f, 0.0f, 1.0f);
    }
}

void W3DViewMainWindow::toggleMungeSortOnLoad(bool enabled)
{
    WW3D::Enable_Munge_Sort_On_Load(enabled);
    QSettings settings;
    settings.setValue("Config/MungeSortOnLoad", enabled ? 1 : 0);
}

void W3DViewMainWindow::openBackgroundObjectDialog()
{
    if (!_viewport) {
        return;
    }

    BackgroundObjectDialog dialog(_viewport->backgroundObjectName(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString name = dialog.selectedName();
    _viewport->setBackgroundObjectName(name);
    if (name.isEmpty()) {
        statusBar()->showMessage("Background object cleared.");
    } else {
        statusBar()->showMessage(QString("Background object: %1").arg(name));
    }
}

void W3DViewMainWindow::captureScreenshot()
{
    const QString base = QDir(QCoreApplication::applicationDirPath()).filePath("ScreenShot");
    const QByteArray native = QDir::toNativeSeparators(base).toLocal8Bit();
    WW3D::Make_Screen_Shot(native.constData());
    statusBar()->showMessage(QString("Saved screenshot: %1").arg(base));
}

void W3DViewMainWindow::makeMovie()
{
    if (!_viewport || !_treeView) {
        return;
    }

    const QModelIndex current = _treeView->currentIndex();
    if (!current.isValid() ||
        current.data(kRoleType).toInt() != static_cast<int>(AssetNodeType::Animation)) {
        QMessageBox::information(this, "Make Movie", "Select an animation to capture.");
        return;
    }

    if (!_viewport->hasAnimation()) {
        QMessageBox::information(this, "Make Movie", "No animation is available for capture.");
        return;
    }

    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QGuiApplication::setOverrideCursor(Qt::BlankCursor);
    QString error;
    const bool ok = _viewport->captureMovie(QStringLiteral("Grab"), 30.0f, &error);
    QGuiApplication::restoreOverrideCursor();

    if (!ok) {
        const QString message = error.isEmpty() ? "Movie capture failed." : error;
        QMessageBox::warning(this, "Make Movie", message);
        return;
    }

    statusBar()->showMessage("Movie capture complete.");
}

void W3DViewMainWindow::selectPrevAsset()
{
    if (!_treeView) {
        return;
    }

    const QModelIndex current = _treeView->currentIndex();
    if (!current.isValid()) {
        return;
    }

    const QModelIndex prev = _treeModel->index(current.row() - 1, current.column(), current.parent());
    if (prev.isValid()) {
        _treeView->setCurrentIndex(prev);
    }
}

void W3DViewMainWindow::selectNextAsset()
{
    if (!_treeView) {
        return;
    }

    const QModelIndex current = _treeView->currentIndex();
    if (!current.isValid()) {
        return;
    }

    const QModelIndex next = _treeModel->index(current.row() + 1, current.column(), current.parent());
    if (next.isValid()) {
        _treeView->setCurrentIndex(next);
    }
}

void W3DViewMainWindow::showTreeContextMenu(const QPoint &pos)
{
    if (!_treeView || !_viewport) {
        return;
    }

    const QModelIndex index = _treeView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    const int type_value = index.data(kRoleType).toInt();
    const bool is_group = type_value == static_cast<int>(AssetNodeType::Group);
    if (!is_group) {
        _treeView->setCurrentIndex(index);
    }
    auto matches_group = [](const QString &text, const QString &label) {
        return text == label || text.startsWith(label + " (");
    };

    if (type_value == static_cast<int>(AssetNodeType::Animation)) {
        QMenu menu(this);
        auto *play_action = menu.addAction("Play");
        connect(play_action, &QAction::triggered, this, &W3DViewMainWindow::startAnimation);
        auto *pause_action = menu.addAction("Pause");
        connect(pause_action, &QAction::triggered, this, &W3DViewMainWindow::pauseAnimation);
        auto *stop_action = menu.addAction("Stop");
        connect(stop_action, &QAction::triggered, this, &W3DViewMainWindow::stopAnimation);
        menu.addSeparator();
        auto *step_back_action = menu.addAction("Step Back");
        connect(step_back_action, &QAction::triggered, this, &W3DViewMainWindow::stepAnimationBackward);
        auto *step_forward_action = menu.addAction("Step Forward");
        connect(step_forward_action, &QAction::triggered, this, &W3DViewMainWindow::stepAnimationForward);
        menu.addSeparator();
        auto *settings_action = menu.addAction("Settings");
        connect(settings_action, &QAction::triggered, this, &W3DViewMainWindow::openAnimationSettings);
        menu.addSeparator();
        auto *advanced_action = menu.addAction("Advanced...");
        connect(advanced_action, &QAction::triggered, this, &W3DViewMainWindow::openAdvancedAnimation);

        const bool has_anim = _viewport->hasAnimation();
        play_action->setEnabled(has_anim);
        pause_action->setEnabled(has_anim);
        stop_action->setEnabled(has_anim);
        step_back_action->setEnabled(has_anim);
        step_forward_action->setEnabled(has_anim);

        menu.exec(_treeView->viewport()->mapToGlobal(pos));
        return;
    }

    if (type_value != static_cast<int>(AssetNodeType::RenderObject) && !is_group) {
        return;
    }

    const QString group_label = ResolveGroupLabel(_treeModel, index, is_group);

    if (matches_group(group_label, "H-LOD")) {
        QMenu menu(this);
        auto *record_action = menu.addAction("Record Screen Area");
        connect(record_action, &QAction::triggered, this, &W3DViewMainWindow::recordLodScreenArea);

        auto *include_null_action = menu.addAction("Include NULL Object");
        include_null_action->setCheckable(true);
        include_null_action->setChecked(_viewport->isNullLodIncluded());
        connect(include_null_action, &QAction::triggered, this, &W3DViewMainWindow::toggleLodIncludeNull);

        menu.addSeparator();

        auto *prev_action = menu.addAction("Prev Level");
        connect(prev_action, &QAction::triggered, this, &W3DViewMainWindow::selectPrevLod);
        auto *next_action = menu.addAction("Next Level");
        connect(next_action, &QAction::triggered, this, &W3DViewMainWindow::selectNextLod);

        int level = 0;
        int count = 0;
        if (_viewport->currentLodInfo(level, count)) {
            prev_action->setEnabled(level > 0);
            next_action->setEnabled(level + 1 < count);
        }

        menu.addSeparator();

        auto *auto_switch_action = menu.addAction("Auto Switching");
        auto_switch_action->setCheckable(true);
        auto_switch_action->setChecked(_viewport->isLodAutoSwitchingEnabled());
        connect(auto_switch_action, &QAction::triggered, this, &W3DViewMainWindow::toggleLodAutoSwitch);

        menu.addSeparator();
        auto *make_aggregate_action = menu.addAction("Make Aggregate...");
        connect(make_aggregate_action, &QAction::triggered, this, &W3DViewMainWindow::makeAggregate);

        menu.exec(_treeView->viewport()->mapToGlobal(pos));
        return;
    }

    if (matches_group(group_label, "Hierarchy")) {
        QMenu menu(this);
        auto *generate_lod_action = menu.addAction("Generate LOD...");
        connect(generate_lod_action, &QAction::triggered, this, &W3DViewMainWindow::generateLod);
        auto *make_aggregate_action = menu.addAction("Make Aggregate...");
        connect(make_aggregate_action, &QAction::triggered, this, &W3DViewMainWindow::makeAggregate);
        menu.exec(_treeView->viewport()->mapToGlobal(pos));
        return;
    }

    if (matches_group(group_label, "Aggregate")) {
        QMenu menu(this);
        auto *rename_action = menu.addAction("Rename Aggregate...");
        connect(rename_action, &QAction::triggered, this, &W3DViewMainWindow::renameAggregate);
        menu.addSeparator();
        auto *bone_action = menu.addAction("Bone Management...");
        connect(bone_action, &QAction::triggered, this, &W3DViewMainWindow::openBoneManagement);
        auto *auto_assign_action = menu.addAction("Auto Assign Bone Models");
        connect(auto_assign_action, &QAction::triggered, this, &W3DViewMainWindow::autoAssignBoneModels);
        menu.addSeparator();
        auto *bind_action = menu.addAction("Bind Subobject LOD");
        bind_action->setCheckable(true);
        bind_action->setChecked(_viewport->isSubobjectLodBound());
        connect(bind_action, &QAction::triggered, this, &W3DViewMainWindow::bindSubobjectLod);
        auto *generate_lod_action = menu.addAction("Generate LOD...");
        connect(generate_lod_action, &QAction::triggered, this, &W3DViewMainWindow::generateLod);
        menu.exec(_treeView->viewport()->mapToGlobal(pos));
        return;
    }
}

void W3DViewMainWindow::startAnimation()
{
    if (_viewport) {
        _viewport->setAnimationState(W3DViewport::AnimationState::Playing);
        statusBar()->showMessage("Animation playing.");
    }
}

void W3DViewMainWindow::pauseAnimation()
{
    if (!_viewport) {
        return;
    }

    const auto state = _viewport->animationState();
    if (state == W3DViewport::AnimationState::Playing) {
        _viewport->setAnimationState(W3DViewport::AnimationState::Paused);
        statusBar()->showMessage("Animation paused.");
    } else if (state == W3DViewport::AnimationState::Paused) {
        _viewport->setAnimationState(W3DViewport::AnimationState::Playing);
        statusBar()->showMessage("Animation resumed.");
    }
}

void W3DViewMainWindow::stopAnimation()
{
    if (_viewport) {
        _viewport->setAnimationState(W3DViewport::AnimationState::Stopped);
        statusBar()->showMessage("Animation stopped.");
    }
}

void W3DViewMainWindow::stepAnimationForward()
{
    if (!_viewport) {
        return;
    }

    if (!_viewport->stepAnimation(1)) {
        statusBar()->showMessage("No animation to step.");
    }
}

void W3DViewMainWindow::stepAnimationBackward()
{
    if (!_viewport) {
        return;
    }

    if (!_viewport->stepAnimation(-1)) {
        statusBar()->showMessage("No animation to step.");
    }
}

void W3DViewMainWindow::openAnimationSettings()
{
    if (!_viewport) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Animation Settings");

    auto *layout = new QVBoxLayout(&dialog);
    auto *speed_label = new QLabel(&dialog);
    auto *speed_slider = new QSlider(Qt::Horizontal, &dialog);
    auto *blend_checkbox = new QCheckBox("Blend frames", &dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    const float initial_speed = _viewport->animationSpeed();
    speed_label->setText(QString("Speed: %1x").arg(initial_speed, 0, 'f', 2));

    speed_slider->setRange(1, 200);
    const int initial_percent = std::clamp(static_cast<int>(initial_speed * 100.0f + 0.5f), 1, 200);
    speed_slider->setValue(initial_percent);

    blend_checkbox->setChecked(_viewport->animationBlend());

    layout->addWidget(speed_label);
    layout->addWidget(speed_slider);
    layout->addWidget(blend_checkbox);
    layout->addWidget(buttons);

    bool slider_touched = false;
    connect(speed_slider, &QSlider::valueChanged, &dialog, [this, speed_label, &slider_touched](int value) {
        slider_touched = true;
        const float speed = static_cast<float>(value) / 100.0f;
        speed_label->setText(QString("Speed: %1x").arg(speed, 0, 'f', 2));
        _viewport->setAnimationSpeed(speed);
    });
    connect(blend_checkbox, &QCheckBox::toggled, &dialog, [this](bool checked) {
        _viewport->setAnimationBlend(checked);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted && slider_touched) {
        _viewport->setAnimationSpeed(initial_speed);
    }
}

void W3DViewMainWindow::openAdvancedAnimation()
{
    if (!_viewport || !_treeView) {
        return;
    }

    QString name;
    if (!GetSelectedRenderObjectName(_treeView, name)) {
        QMessageBox::information(this,
                                 "Advanced Animation",
                                 "Select a render object or animation before opening advanced controls.");
        return;
    }

    AdvancedAnimationDialog dialog(_viewport, name, this);
    if (dialog.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Applied advanced animation mix.");
    }
}

void W3DViewMainWindow::generateLod()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Generate LOD", "Select a hierarchy to generate an LOD.");
        return;
    }
    Q_UNUSED(class_id);

    LodNamingType type = LodNamingType::Commando;
    if (!IsLodNameValid(name, type)) {
        QMessageBox::information(this,
                                 "Generate LOD",
                                 "Selected hierarchy name does not match LOD naming conventions.");
        return;
    }

    QString base_name = name;
    if (type == LodNamingType::Commando) {
        base_name.chop(2);
    } else {
        base_name.chop(1);
    }

    HLodPrototypeClass *prototype = GenerateLodPrototype(base_name, type);
    if (!prototype) {
        QMessageBox::warning(this, "Generate LOD", "Failed to generate LOD.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        delete prototype;
        QMessageBox::warning(this, "Generate LOD", "WW3D asset manager is not available.");
        return;
    }

    asset_manager->Add_Prototype(prototype);
    rebuildAssetTree();
    statusBar()->showMessage(QString("Generated LOD: %1").arg(prototype->Get_Name()));
}

void W3DViewMainWindow::makeAggregate()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Make Aggregate", "Select a hierarchy to make an aggregate.");
        return;
    }
    Q_UNUSED(class_id);

    AggregateNameDialog dialog("Make Aggregate", QString(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString aggregate_name = dialog.name();
    if (aggregate_name.isEmpty()) {
        QMessageBox::information(this, "Make Aggregate", "Aggregate name is required.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Make Aggregate", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Make Aggregate", "Failed to load hierarchy.");
        return;
    }

    auto *definition = new AggregateDefClass(*render_obj);
    const QByteArray aggregate_bytes = aggregate_name.toLatin1();
    definition->Set_Name(aggregate_bytes.constData());
    auto *prototype = new AggregatePrototypeClass(definition);

    asset_manager->Remove_Prototype(definition->Get_Name());
    asset_manager->Add_Prototype(prototype);

    render_obj->Release_Ref();
    rebuildAssetTree();
    statusBar()->showMessage(QString("Created aggregate: %1").arg(aggregate_name));
}

void W3DViewMainWindow::renameAggregate()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Rename Aggregate", "Select an aggregate to rename.");
        return;
    }
    Q_UNUSED(class_id);

    const RenderObjInfo info = InspectRenderObj(name.toLatin1().constData());
    if (!info.isAggregate) {
        QMessageBox::information(this, "Rename Aggregate", "Selected object is not an aggregate.");
        return;
    }

    AggregateNameDialog dialog("Rename Aggregate", name, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString new_name = dialog.name();
    if (new_name.isEmpty()) {
        QMessageBox::information(this, "Rename Aggregate", "Aggregate name is required.");
        return;
    }

    if (!RenameAggregatePrototype(name.toLatin1().constData(), new_name.toLatin1().constData())) {
        QMessageBox::warning(this, "Rename Aggregate", "Failed to rename aggregate.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (_viewport && asset_manager) {
        const QByteArray new_bytes = new_name.toLatin1();
        RenderObjClass *render_obj = asset_manager->Create_Render_Obj(new_bytes.constData());
        if (render_obj) {
            _viewport->clearAnimation();
            _viewport->setRenderObject(render_obj);
            render_obj->Release_Ref();
        }
    }

    rebuildAssetTree();
    statusBar()->showMessage(QString("Renamed aggregate: %1").arg(new_name));
}

void W3DViewMainWindow::openBoneManagement()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Bone Management", "Select an aggregate to edit bones.");
        return;
    }
    Q_UNUSED(class_id);

    const RenderObjInfo info = InspectRenderObj(name.toLatin1().constData());
    if (!info.isAggregate) {
        QMessageBox::information(this, "Bone Management", "Selected object is not an aggregate.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Bone Management", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Bone Management", "Failed to load aggregate.");
        return;
    }

    if (_viewport) {
        _viewport->clearAnimation();
        _viewport->setRenderObject(render_obj);
    }

    BoneManagementDialog dialog(render_obj, _viewport, this);
    const int result = dialog.exec();
    render_obj->Release_Ref();

    if (result == QDialog::Accepted) {
        statusBar()->showMessage("Updated aggregate bones.");
    }
}

void W3DViewMainWindow::autoAssignBoneModels()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Auto Assign Bones", "Select an aggregate to assign bones.");
        return;
    }
    Q_UNUSED(class_id);

    const RenderObjInfo info = InspectRenderObj(name.toLatin1().constData());
    if (!info.isAggregate) {
        QMessageBox::information(this, "Auto Assign Bones", "Selected object is not an aggregate.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Auto Assign Bones", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Auto Assign Bones", "Failed to load aggregate.");
        return;
    }

    bool updated = false;
    const int bone_count = render_obj->Get_Num_Bones();
    for (int index = 0; index < bone_count; ++index) {
        const char *bone_name = render_obj->Get_Bone_Name(index);
        if (!bone_name || !bone_name[0]) {
            continue;
        }

        if (!asset_manager->Render_Obj_Exists(bone_name)) {
            continue;
        }

        RenderObjClass *bone_obj = asset_manager->Create_Render_Obj(bone_name);
        if (!bone_obj) {
            continue;
        }

        render_obj->Add_Sub_Object_To_Bone(bone_obj, index);
        bone_obj->Release_Ref();
        updated = true;
    }

    if (updated) {
        UpdateAggregatePrototype(*render_obj);
    }

    if (_viewport) {
        _viewport->clearAnimation();
        _viewport->setRenderObject(render_obj);
    }

    render_obj->Release_Ref();
    statusBar()->showMessage(updated ? "Auto assigned bone models." : "No matching bone models found.");
}

void W3DViewMainWindow::bindSubobjectLod()
{
    if (!_viewport) {
        return;
    }

    const bool enabled = _viewport->toggleSubobjectLod();
    statusBar()->showMessage(enabled ? "Subobject LOD binding enabled." : "Subobject LOD binding disabled.");
}

void W3DViewMainWindow::createEmitter()
{
    ParticleEmitterDefClass definition = CreateDefaultEmitterDefinition();
    EmitterEditDialog dialog(definition, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::unique_ptr<ParticleEmitterDefClass> updated(dialog.definition());
    if (!updated) {
        QMessageBox::warning(this, "Create Emitter", "Failed to create emitter definition.");
        return;
    }

    if (!UpdateEmitterPrototype(*updated, dialog.originalName())) {
        QMessageBox::warning(this, "Create Emitter", "Failed to register emitter prototype.");
        return;
    }

    const char *name = updated->Get_Name();
    if (name && name[0] && _viewport) {
        auto *asset_manager = WW3DAssetManager::Get_Instance();
        if (asset_manager) {
            RenderObjClass *object = asset_manager->Create_Render_Obj(name);
            if (object) {
                _viewport->clearAnimation();
                _viewport->setRenderObject(object);
                object->Release_Ref();
            }
        }
    }

    rebuildAssetTree();
    statusBar()->showMessage("Created emitter.");
}

void W3DViewMainWindow::scaleEmitter()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Scale Emitter", "Select an emitter to scale.");
        return;
    }
    if (class_id != RenderObjClass::CLASSID_PARTICLEEMITTER) {
        QMessageBox::information(this, "Scale Emitter", "Selected object is not an emitter.");
        return;
    }

    ScaleDialog dialog(1.0, "Enter the scaling factor you want to apply to the current particle emitter.", this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Scale Emitter", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Scale Emitter", "Failed to load emitter.");
        return;
    }

    if (render_obj->Class_ID() != RenderObjClass::CLASSID_PARTICLEEMITTER) {
        render_obj->Release_Ref();
        QMessageBox::warning(this, "Scale Emitter", "Selected object is not an emitter.");
        return;
    }

    auto *emitter = static_cast<ParticleEmitterClass *>(render_obj);
    emitter->Scale(static_cast<float>(dialog.scale()));

    ParticleEmitterDefClass *definition = emitter->Build_Definition();
    if (!definition) {
        emitter->Release_Ref();
        QMessageBox::warning(this, "Scale Emitter", "Failed to update emitter definition.");
        return;
    }

    UpdateEmitterPrototype(*definition, name);
    delete definition;

    if (_viewport) {
        _viewport->clearAnimation();
        _viewport->setRenderObject(emitter);
    }
    emitter->Release_Ref();
    statusBar()->showMessage("Scaled emitter.");
}

void W3DViewMainWindow::editEmitter()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Edit Emitter", "Select an emitter to edit.");
        return;
    }
    if (class_id != RenderObjClass::CLASSID_PARTICLEEMITTER) {
        QMessageBox::information(this, "Edit Emitter", "Selected object is not an emitter.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Edit Emitter", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to load emitter.");
        return;
    }

    if (render_obj->Class_ID() != RenderObjClass::CLASSID_PARTICLEEMITTER) {
        render_obj->Release_Ref();
        QMessageBox::warning(this, "Edit Emitter", "Selected object is not an emitter.");
        return;
    }

    auto *emitter = static_cast<ParticleEmitterClass *>(render_obj);
    ParticleEmitterDefClass *definition = emitter->Build_Definition();
    emitter->Release_Ref();
    if (!definition) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to load emitter definition.");
        return;
    }

    EmitterEditDialog dialog(*definition, this);
    delete definition;
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::unique_ptr<ParticleEmitterDefClass> updated(dialog.definition());
    if (!updated) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to update emitter definition.");
        return;
    }

    if (!UpdateEmitterPrototype(*updated, dialog.originalName())) {
        QMessageBox::warning(this, "Edit Emitter", "Failed to register emitter prototype.");
        return;
    }

    QString updated_name = name;
    if (updated->Get_Name() && updated->Get_Name()[0]) {
        updated_name = QString::fromLatin1(updated->Get_Name());
    }

    if (updated->Get_Name() && updated->Get_Name()[0] && _viewport) {
        RenderObjClass *object = asset_manager->Create_Render_Obj(updated->Get_Name());
        if (object) {
            _viewport->clearAnimation();
            _viewport->setRenderObject(object);
            object->Release_Ref();
        }
    }

    rebuildAssetTree();
    if (_treeModel && _treeView) {
        const QModelIndex index = FindRenderObjectIndex(_treeModel,
                                                        updated_name,
                                                        RenderObjClass::CLASSID_PARTICLEEMITTER);
        if (index.isValid()) {
            ExpandParentChain(_treeView, index.parent());
            _treeView->setCurrentIndex(index);
            _treeView->scrollTo(index);
        }
    }
    statusBar()->showMessage("Updated emitter.");
}

void W3DViewMainWindow::createSphere()
{
    if (!_viewport) {
        return;
    }

    _viewport->clearAnimation();
    _viewport->setRenderObject(nullptr);

    SphereEditDialog dialog(nullptr, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    SphereRenderObjClass *sphere = dialog.sphere();
    if (!sphere) {
        QMessageBox::warning(this, "Create Sphere", "Failed to create sphere.");
        return;
    }

    if (!UpdateSpherePrototype(sphere, dialog.oldName())) {
        sphere->Release_Ref();
        QMessageBox::warning(this, "Create Sphere", "Failed to register sphere prototype.");
        return;
    }

    rebuildAssetTree();
    _viewport->clearAnimation();
    _viewport->setRenderObject(sphere);
    const char *name = sphere->Get_Name();
    statusBar()->showMessage(QString("Created sphere: %1").arg(name ? name : ""));
    sphere->Release_Ref();
}

void W3DViewMainWindow::createRing()
{
    if (!_viewport) {
        return;
    }

    _viewport->clearAnimation();
    _viewport->setRenderObject(nullptr);

    RingEditDialog dialog(nullptr, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    RingRenderObjClass *ring = dialog.ring();
    if (!ring) {
        QMessageBox::warning(this, "Create Ring", "Failed to create ring.");
        return;
    }

    if (!UpdateRingPrototype(ring, dialog.oldName())) {
        ring->Release_Ref();
        QMessageBox::warning(this, "Create Ring", "Failed to register ring prototype.");
        return;
    }

    rebuildAssetTree();
    _viewport->clearAnimation();
    _viewport->setRenderObject(ring);
    const char *name = ring->Get_Name();
    statusBar()->showMessage(QString("Created ring: %1").arg(name ? name : ""));
    ring->Release_Ref();
}

void W3DViewMainWindow::editPrimitive()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Edit Primitive", "Select a primitive to edit.");
        return;
    }
    if (class_id != RenderObjClass::CLASSID_SPHERE &&
        class_id != RenderObjClass::CLASSID_RING) {
        QMessageBox::information(this, "Edit Primitive", "Selected object is not a primitive.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Edit Primitive", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Edit Primitive", "Failed to load primitive.");
        return;
    }

    if (class_id == RenderObjClass::CLASSID_SPHERE) {
        if (render_obj->Class_ID() != RenderObjClass::CLASSID_SPHERE) {
            render_obj->Release_Ref();
            QMessageBox::warning(this, "Edit Primitive", "Selected object is not a sphere.");
            return;
        }

        auto *sphere = static_cast<SphereRenderObjClass *>(render_obj);
        SphereEditDialog dialog(sphere, this);
        sphere->Release_Ref();
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        SphereRenderObjClass *updated = dialog.sphere();
        if (!updated) {
            QMessageBox::warning(this, "Edit Primitive", "Failed to update sphere.");
            return;
        }

        if (!UpdateSpherePrototype(updated, dialog.oldName())) {
            updated->Release_Ref();
            QMessageBox::warning(this, "Edit Primitive", "Failed to register sphere prototype.");
            return;
        }

        rebuildAssetTree();
        if (_viewport) {
            _viewport->clearAnimation();
            _viewport->setRenderObject(updated);
        }
        statusBar()->showMessage("Updated sphere.");
        updated->Release_Ref();
        return;
    }

    if (render_obj->Class_ID() != RenderObjClass::CLASSID_RING) {
        render_obj->Release_Ref();
        QMessageBox::warning(this, "Edit Primitive", "Selected object is not a ring.");
        return;
    }

    auto *ring = static_cast<RingRenderObjClass *>(render_obj);
    RingEditDialog dialog(ring, this);
    ring->Release_Ref();
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    RingRenderObjClass *updated = dialog.ring();
    if (!updated) {
        QMessageBox::warning(this, "Edit Primitive", "Failed to update ring.");
        return;
    }

    if (!UpdateRingPrototype(updated, dialog.oldName())) {
        updated->Release_Ref();
        QMessageBox::warning(this, "Edit Primitive", "Failed to register ring prototype.");
        return;
    }

    rebuildAssetTree();
    if (_viewport) {
        _viewport->clearAnimation();
        _viewport->setRenderObject(updated);
    }
    statusBar()->showMessage("Updated ring.");
    updated->Release_Ref();
}

void W3DViewMainWindow::createSoundObject()
{
    SoundEditDialog dialog(nullptr, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    SoundRenderObjClass *sound_obj = dialog.sound();
    if (!sound_obj) {
        QMessageBox::warning(this, "Create Sound Object", "Failed to create sound object.");
        return;
    }

    if (!UpdateSoundPrototype(sound_obj, dialog.oldName())) {
        sound_obj->Release_Ref();
        QMessageBox::warning(this, "Create Sound Object", "Failed to register sound object.");
        return;
    }

    rebuildAssetTree();
    if (_viewport) {
        _viewport->clearAnimation();
        _viewport->setRenderObject(sound_obj);
    }
    const char *created_name = sound_obj->Get_Name();
    statusBar()->showMessage(QString("Created sound object: %1").arg(created_name ? created_name : ""));
    sound_obj->Release_Ref();
}

void W3DViewMainWindow::editSoundObject()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Edit Sound Object", "Select a sound object to edit.");
        return;
    }
    if (class_id != RenderObjClass::CLASSID_SOUND) {
        QMessageBox::information(this, "Edit Sound Object", "Selected object is not a sound object.");
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Edit Sound Object", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *render_obj = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!render_obj) {
        QMessageBox::warning(this, "Edit Sound Object", "Failed to load sound object.");
        return;
    }

    if (render_obj->Class_ID() != RenderObjClass::CLASSID_SOUND) {
        render_obj->Release_Ref();
        QMessageBox::warning(this, "Edit Sound Object", "Selected object is not a sound object.");
        return;
    }

    auto *sound_obj = static_cast<SoundRenderObjClass *>(render_obj);
    SoundEditDialog dialog(sound_obj, this);
    const int result = dialog.exec();
    sound_obj->Release_Ref();

    if (result != QDialog::Accepted) {
        return;
    }

    SoundRenderObjClass *updated = dialog.sound();
    if (!updated) {
        QMessageBox::warning(this, "Edit Sound Object", "Failed to update sound object.");
        return;
    }

    if (!UpdateSoundPrototype(updated, dialog.oldName())) {
        updated->Release_Ref();
        QMessageBox::warning(this, "Edit Sound Object", "Failed to register sound object.");
        return;
    }

    rebuildAssetTree();
    if (_viewport) {
        _viewport->clearAnimation();
        _viewport->setRenderObject(updated);
    }
    const char *updated_name = updated->Get_Name();
    statusBar()->showMessage(QString("Updated sound object: %1").arg(updated_name ? updated_name : ""));
    updated->Release_Ref();
}

void W3DViewMainWindow::openAnimatedSoundOptions()
{
    QSettings settings;
    const QString definition_path = settings.value("Config/SoundDefLibPath").toString();
    const QString ini_path = settings.value("Config/AnimSoundINIPath").toString();
    const QString data_path = settings.value("Config/AnimSoundDataPath").toString();

    AnimatedSoundOptionsDialog dialog(definition_path, ini_path, data_path, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto normalize = [](const QString &value) -> QString {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty()) {
            return QString();
        }
        return QDir::toNativeSeparators(QDir::cleanPath(trimmed));
    };

    const QString new_definition = normalize(dialog.definitionLibraryPath());
    const QString new_ini = normalize(dialog.iniPath());
    const QString new_data = normalize(dialog.dataPath());

    settings.setValue("Config/SoundDefLibPath", new_definition);
    settings.setValue("Config/AnimSoundINIPath", new_ini);
    settings.setValue("Config/AnimSoundDataPath", new_data);

    AnimatedSoundOptionsDialog::LoadAnimatedSoundSettings();
    statusBar()->showMessage("Animated sound options updated.");
}

void W3DViewMainWindow::importFacialAnims()
{
    if (!_treeView) {
        return;
    }

    const QString hierarchy = GetSelectedHierarchyName(_treeView);
    if (hierarchy.isEmpty()) {
        QMessageBox::information(this, "Import Facial Anims", "Select a hierarchy before importing.");
        return;
    }

    const QString start_dir = _lastOpenedPath.isEmpty() ? QDir::currentPath() : _lastOpenedPath;
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Import Facial Anims",
        start_dir,
        "Animation Description (*.txt)");
    if (files.isEmpty()) {
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    int imported = 0;
    for (const QString &path : files) {
        if (ImportFacialAnimation(hierarchy, path)) {
            ++imported;
        }
    }
    QGuiApplication::restoreOverrideCursor();

    if (imported > 0) {
        rebuildAssetTree();
        statusBar()->showMessage(QString("Imported %1 facial animation(s).").arg(imported));
    } else {
        QMessageBox::warning(this, "Import Facial Anims", "No facial animations were imported.");
    }
}

void W3DViewMainWindow::exportAggregate()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Export Aggregate", "Select an aggregate to export.");
        return;
    }

    const RenderObjInfo info = InspectRenderObj(name.toLatin1().constData());
    if (!info.isAggregate) {
        QMessageBox::information(this, "Export Aggregate", "Selected object is not an aggregate.");
        return;
    }

    const QString base_dir = _lastOpenedPath.isEmpty() ? QDir::currentPath() : _lastOpenedPath;
    const QString default_path = QDir(base_dir).filePath(name + ".w3d");
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export Aggregate",
        default_path,
        "Westwood 3D Files (*.w3d);;All Files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Export Aggregate", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    auto *proto = static_cast<AggregatePrototypeClass *>(
        asset_manager->Find_Prototype(name_bytes.constData()));
    if (!proto) {
        QMessageBox::warning(this, "Export Aggregate", "Aggregate prototype not found.");
        return;
    }

    AggregateDefClass *definition = proto->Get_Definition();
    if (!definition) {
        QMessageBox::warning(this, "Export Aggregate", "Aggregate definition not available.");
        return;
    }

    const bool ok = SaveChunkToFile(path, [definition](ChunkSaveClass &save_chunk) {
        return definition->Save_W3D(save_chunk) == WW3D_ERROR_OK;
    });
    if (!ok) {
        QMessageBox::warning(this, "Export Aggregate", "Failed to export aggregate.");
        return;
    }

    statusBar()->showMessage(QString("Exported aggregate: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::exportEmitter()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Export Emitter", "Select an emitter to export.");
        return;
    }

    if (class_id != RenderObjClass::CLASSID_PARTICLEEMITTER) {
        QMessageBox::information(this, "Export Emitter", "Selected object is not an emitter.");
        return;
    }

    const QString base_dir = _lastOpenedPath.isEmpty() ? QDir::currentPath() : _lastOpenedPath;
    const QString default_path = QDir(base_dir).filePath(name + ".w3d");
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export Emitter",
        default_path,
        "Westwood 3D Files (*.w3d);;All Files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Export Emitter", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    auto *proto = static_cast<ParticleEmitterPrototypeClass *>(
        asset_manager->Find_Prototype(name_bytes.constData()));
    if (!proto) {
        QMessageBox::warning(this, "Export Emitter", "Emitter prototype not found.");
        return;
    }

    ParticleEmitterDefClass *definition = proto->Get_Definition();
    if (!definition) {
        QMessageBox::warning(this, "Export Emitter", "Emitter definition not available.");
        return;
    }

    const bool ok = SaveChunkToFile(path, [definition](ChunkSaveClass &save_chunk) {
        return definition->Save_W3D(save_chunk) == WW3D_ERROR_OK;
    });
    if (!ok) {
        QMessageBox::warning(this, "Export Emitter", "Failed to export emitter.");
        return;
    }

    statusBar()->showMessage(QString("Exported emitter: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::exportLod()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Export LOD", "Select an LOD to export.");
        return;
    }

    if (class_id != RenderObjClass::CLASSID_HLOD) {
        QMessageBox::information(this, "Export LOD", "Selected object is not an LOD.");
        return;
    }

    const QString base_dir = _lastOpenedPath.isEmpty() ? QDir::currentPath() : _lastOpenedPath;
    const QString default_path = QDir(base_dir).filePath(name + ".w3d");
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export LOD",
        default_path,
        "Westwood 3D Files (*.w3d);;All Files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Export LOD", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    auto *proto = static_cast<HLodPrototypeClass *>(
        asset_manager->Find_Prototype(name_bytes.constData()));
    if (!proto) {
        QMessageBox::warning(this, "Export LOD", "LOD prototype not found.");
        return;
    }

    HLodDefClass *definition = proto->Get_Definition();
    if (!definition) {
        QMessageBox::warning(this, "Export LOD", "LOD definition not available.");
        return;
    }

    const bool ok = SaveChunkToFile(path, [definition](ChunkSaveClass &save_chunk) {
        return definition->Save(save_chunk) == WW3D_ERROR_OK;
    });
    if (!ok) {
        QMessageBox::warning(this, "Export LOD", "Failed to export LOD.");
        return;
    }

    statusBar()->showMessage(QString("Exported LOD: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::exportPrimitive()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Export Primitive", "Select a primitive to export.");
        return;
    }

    if (class_id != RenderObjClass::CLASSID_SPHERE &&
        class_id != RenderObjClass::CLASSID_RING) {
        QMessageBox::information(this, "Export Primitive", "Selected object is not a primitive.");
        return;
    }

    const QString base_dir = _lastOpenedPath.isEmpty() ? QDir::currentPath() : _lastOpenedPath;
    const QString default_path = QDir(base_dir).filePath(name + ".w3d");
    const QString title = class_id == RenderObjClass::CLASSID_SPHERE ? "Export Sphere" : "Export Ring";
    const QString path = QFileDialog::getSaveFileName(
        this,
        title,
        default_path,
        "Westwood 3D Files (*.w3d);;All Files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Export Primitive", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    bool ok = false;
    if (class_id == RenderObjClass::CLASSID_SPHERE) {
        auto *proto = static_cast<SpherePrototypeClass *>(
            asset_manager->Find_Prototype(name_bytes.constData()));
        if (!proto) {
            QMessageBox::warning(this, "Export Primitive", "Sphere prototype not found.");
            return;
        }
        ok = SaveChunkToFile(path, [proto](ChunkSaveClass &save_chunk) {
            return proto->Save(save_chunk);
        });
    } else {
        auto *proto = static_cast<RingPrototypeClass *>(
            asset_manager->Find_Prototype(name_bytes.constData()));
        if (!proto) {
            QMessageBox::warning(this, "Export Primitive", "Ring prototype not found.");
            return;
        }
        ok = SaveChunkToFile(path, [proto](ChunkSaveClass &save_chunk) {
            return proto->Save(save_chunk);
        });
    }

    if (!ok) {
        QMessageBox::warning(this, "Export Primitive", "Failed to export primitive.");
        return;
    }

    statusBar()->showMessage(QString("Exported primitive: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::exportSoundObject()
{
    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Export Sound Object", "Select a sound object to export.");
        return;
    }

    if (class_id != RenderObjClass::CLASSID_SOUND) {
        QMessageBox::information(this, "Export Sound Object", "Selected object is not a sound object.");
        return;
    }

    const QString base_dir = _lastOpenedPath.isEmpty() ? QDir::currentPath() : _lastOpenedPath;
    const QString default_path = QDir(base_dir).filePath(name + ".w3d");
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Export Sound Object",
        default_path,
        "Westwood 3D Files (*.w3d);;All Files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Export Sound Object", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    auto *proto = static_cast<SoundRenderObjPrototypeClass *>(
        asset_manager->Find_Prototype(name_bytes.constData()));
    if (!proto) {
        QMessageBox::warning(this, "Export Sound Object", "Sound object prototype not found.");
        return;
    }

    SoundRenderObjDefClass *definition = proto->Peek_Definition();
    if (!definition) {
        QMessageBox::warning(this, "Export Sound Object", "Sound object definition not available.");
        return;
    }

    const bool ok = SaveChunkToFile(path, [definition](ChunkSaveClass &save_chunk) {
        return definition->Save_W3D(save_chunk) == WW3D_ERROR_OK;
    });
    if (!ok) {
        QMessageBox::warning(this, "Export Sound Object", "Failed to export sound object.");
        return;
    }

    statusBar()->showMessage(QString("Exported sound object: %1").arg(QFileInfo(path).fileName()));
}

void W3DViewMainWindow::listMissingTextures()
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Missing Textures", "WW3D asset manager is not available.");
        return;
    }

    QStringList missing;
    HashTemplateIterator<StringClass, TextureClass *> iterator(asset_manager->Texture_Hash());
    for (iterator.First(); !iterator.Is_Done(); iterator.Next()) {
        auto *texture = iterator.Peek_Value();
        if (!texture || !texture->Is_Missing_Texture()) {
            continue;
        }

        const char *name = iterator.Peek_Key();
        if (name && name[0]) {
            missing.append(QString::fromLatin1(name));
        }
    }

    if (missing.isEmpty()) {
        QMessageBox::information(this, "Texture Info", "No Missing Textures!");
        return;
    }

    QString message("Warning! The following textures are missing:\n\n");
    message += missing.join('\n');
    QMessageBox::warning(this, "Missing Textures", message);
}

void W3DViewMainWindow::copyAssets()
{
    if (!_treeView) {
        return;
    }

    const QModelIndex current = _treeView->currentIndex();
    if (!current.isValid() ||
        current.data(kRoleType).toInt() != static_cast<int>(AssetNodeType::RenderObject)) {
        QMessageBox::information(this, "Copy Asset Files", "Select a render object to copy assets.");
        return;
    }

    if (_lastOpenedPath.isEmpty()) {
        QMessageBox::warning(this, "Copy Asset Files", "No source directory is available.");
        return;
    }

    const QString dest_dir = QFileDialog::getExistingDirectory(
        this,
        "Copy Asset Files",
        _lastOpenedPath);
    if (dest_dir.isEmpty()) {
        return;
    }

    const QString name = current.data(kRoleName).toString();
    if (name.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Copy Asset Files", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *object = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!object) {
        QMessageBox::information(this,
                                 "Copy Asset Files",
                                 QString("Unable to create render object '%1'.").arg(name));
        return;
    }

    DynamicVectorClass<StringClass> dependencies;
    object->Build_Dependency_List(dependencies);
    object->Release_Ref();

    if (dependencies.Count() == 0) {
        QMessageBox::information(this, "Copy Asset Files", "No dependent assets were found.");
        return;
    }

    const QDir src_root(_lastOpenedPath);
    const QDir dest_root(dest_dir);
    QStringList failures;
    for (int index = 0; index < dependencies.Count(); ++index) {
        const char *dep_name = dependencies[index].Peek_Buffer();
        if (!dep_name || !dep_name[0]) {
            continue;
        }

        const QString filename = QString::fromLatin1(dep_name);
        const QString src_path = src_root.filePath(filename);
        const QString dest_path = dest_root.filePath(filename);

        if (!QFile::exists(src_path)) {
            failures.append(src_path);
            continue;
        }

        const QFileInfo dest_info(dest_path);
        QDir dest_dir = dest_info.dir();
        if (!dest_dir.exists() && !dest_dir.mkpath(".")) {
            failures.append(src_path);
            continue;
        }

        if (QFile::exists(dest_path)) {
            QFile::remove(dest_path);
        }

        if (!QFile::copy(src_path, dest_path)) {
            failures.append(src_path);
        }
    }

    if (!failures.isEmpty()) {
        QString message("Unable to copy the following files:\n\n");
        message += failures.join('\n');
        QMessageBox::warning(this, "Copy Failure", message);
        return;
    }

    statusBar()->showMessage(QString("Copied assets to %1").arg(dest_dir));
}

void W3DViewMainWindow::addToLineup()
{
    if (!_viewport) {
        return;
    }

    AddToLineupDialog dialog(_viewport, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString name = dialog.selectedName();
    if (name.isEmpty()) {
        return;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "Add To Lineup", "WW3D asset manager is not available.");
        return;
    }

    const QByteArray name_bytes = name.toLatin1();
    RenderObjClass *object = asset_manager->Create_Render_Obj(name_bytes.constData());
    if (!object) {
        QMessageBox::information(this,
                                 "Add To Lineup",
                                 QString("Unable to create render object '%1'.").arg(name));
        return;
    }

    SetHighestLod(object);
    const bool added = _viewport->addToLineup(object);
    object->Release_Ref();

    if (!added) {
        QMessageBox::information(this, "Add To Lineup", "Selected object cannot be added to the lineup.");
        return;
    }

    statusBar()->showMessage(QString("Added to lineup: %1").arg(name));
}

void W3DViewMainWindow::showAbout()
{
    QMessageBox::about(this,
                       "About W3DViewQt",
                       "W3DViewQt\nQt-based W3D asset viewer.");
}

void W3DViewMainWindow::recordLodScreenArea()
{
    if (!_viewport) {
        return;
    }

    if (_viewport->recordLodScreenArea()) {
        statusBar()->showMessage("Recorded LOD screen area.");
    } else {
        statusBar()->showMessage("LOD screen area is not available.");
    }
}

void W3DViewMainWindow::toggleLodIncludeNull(bool enabled)
{
    if (!_viewport) {
        return;
    }

    if (_viewport->setNullLodIncluded(enabled)) {
        statusBar()->showMessage(enabled ? "Included NULL LOD." : "Removed NULL LOD.");
    } else {
        statusBar()->showMessage("NULL LOD is not available.");
    }
}

void W3DViewMainWindow::selectPrevLod()
{
    if (!_viewport) {
        return;
    }

    if (!_viewport->adjustLodLevel(-1)) {
        statusBar()->showMessage("No previous LOD level.");
    }
}

void W3DViewMainWindow::selectNextLod()
{
    if (!_viewport) {
        return;
    }

    if (!_viewport->adjustLodLevel(1)) {
        statusBar()->showMessage("No next LOD level.");
    }
}

void W3DViewMainWindow::toggleLodAutoSwitch(bool enabled)
{
    if (_viewport) {
        _viewport->setLodAutoSwitchingEnabled(enabled);
        statusBar()->showMessage(enabled ? "LOD auto switching enabled." : "LOD auto switching disabled.");
    }
}

void W3DViewMainWindow::toggleObjectRotateX(bool enabled)
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->objectRotationFlags();
    if (enabled) {
        flags |= W3DViewport::RotateX;
        flags &= ~W3DViewport::RotateXBack;
    } else {
        flags &= ~W3DViewport::RotateX;
    }
    _viewport->setObjectRotationFlags(flags);
}

void W3DViewMainWindow::toggleObjectRotateY(bool enabled)
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->objectRotationFlags();
    if (enabled) {
        flags |= W3DViewport::RotateY;
        flags &= ~W3DViewport::RotateYBack;
    } else {
        flags &= ~W3DViewport::RotateY;
    }
    _viewport->setObjectRotationFlags(flags);
}

void W3DViewMainWindow::toggleObjectRotateYBack()
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->objectRotationFlags();
    flags ^= W3DViewport::RotateYBack;
    flags &= ~W3DViewport::RotateY;
    _viewport->setObjectRotationFlags(flags);
    if (_objectRotateYAction) {
        const QSignalBlocker blocker(_objectRotateYAction);
        _objectRotateYAction->setChecked((flags & W3DViewport::RotateY) != 0);
    }
}

void W3DViewMainWindow::toggleObjectRotateZ(bool enabled)
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->objectRotationFlags();
    if (enabled) {
        flags |= W3DViewport::RotateZ;
        flags &= ~W3DViewport::RotateZBack;
    } else {
        flags &= ~W3DViewport::RotateZ;
    }
    _viewport->setObjectRotationFlags(flags);
}

void W3DViewMainWindow::toggleObjectRotateZBack()
{
    if (!_viewport) {
        return;
    }

    int flags = _viewport->objectRotationFlags();
    flags ^= W3DViewport::RotateZBack;
    flags &= ~W3DViewport::RotateZ;
    _viewport->setObjectRotationFlags(flags);
    if (_objectRotateZAction) {
        const QSignalBlocker blocker(_objectRotateZAction);
        _objectRotateZAction->setChecked((flags & W3DViewport::RotateZ) != 0);
    }
}

void W3DViewMainWindow::resetObject()
{
    if (_viewport) {
        _viewport->resetObjectTransform();
    }
}

void W3DViewMainWindow::toggleAlternateMaterials()
{
    if (_viewport) {
        _viewport->toggleAlternateMaterials();
    }
}

void W3DViewMainWindow::showObjectProperties()
{
    if (!_treeView) {
        return;
    }

    const QModelIndex current = _treeView->currentIndex();
    if (!current.isValid()) {
        QMessageBox::information(this, "Properties", "Select an asset to view properties.");
        return;
    }

    const int type_value = current.data(kRoleType).toInt();
    if (type_value == static_cast<int>(AssetNodeType::Animation)) {
        const QString animation_name = current.data(kRoleName).toString();
        if (animation_name.isEmpty()) {
            QMessageBox::information(this, "Properties", "Select an animation to view properties.");
            return;
        }

        AnimationPropertiesDialog dialog(animation_name, this);
        dialog.exec();
        return;
    }

    if (type_value != static_cast<int>(AssetNodeType::RenderObject)) {
        QMessageBox::information(this, "Properties", "Select a render object to view properties.");
        return;
    }

    QString name;
    int class_id = 0;
    if (!GetSelectedRenderObject(_treeView, name, class_id)) {
        QMessageBox::information(this, "Properties", "Select a render object to view properties.");
        return;
    }

    switch (class_id) {
    case RenderObjClass::CLASSID_MESH:
    {
        MeshPropertiesDialog dialog(name, this);
        dialog.exec();
        return;
    }
    case RenderObjClass::CLASSID_COLLECTION:
    case RenderObjClass::CLASSID_HMODEL:
    case RenderObjClass::CLASSID_DISTLOD:
    case RenderObjClass::CLASSID_HLOD:
    {
        HierarchyPropertiesDialog dialog(name, this);
        dialog.exec();
        return;
    }
    case RenderObjClass::CLASSID_SOUND:
        editSoundObject();
        return;
    case RenderObjClass::CLASSID_PARTICLEEMITTER:
        editEmitter();
        return;
    case RenderObjClass::CLASSID_SPHERE:
    case RenderObjClass::CLASSID_RING:
        editPrimitive();
        return;
    default:
        QMessageBox::information(this,
                                 "Properties",
                                 "Selected object type does not have a properties dialog.");
        return;
    }
}

void W3DViewMainWindow::setNpatchesLevel(int level)
{
    if (level < 1) {
        level = 1;
    }
    if (level > 8) {
        level = 8;
    }

    WW3D::Set_NPatches_Level(static_cast<unsigned int>(level));

    QSettings settings;
    settings.setValue("Config/NPatchesSubdivision", level);
}

void W3DViewMainWindow::toggleNpatchesGap(bool enabled)
{
    WW3D::Set_NPatches_Gap_Filling_Mode(
        enabled ? WW3D::NPATCHES_GAP_FILLING_ENABLED
                : WW3D::NPATCHES_GAP_FILLING_DISABLED);

    QSettings settings;
    settings.setValue("Config/NPatchesGapFilling", enabled ? 1 : 0);
}

void W3DViewMainWindow::reloadLightmapModels()
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager || !_treeModel) {
        return;
    }

    auto *root = _treeModel->invisibleRootItem();
    if (!root) {
        return;
    }

    auto matches_group = [](const QString &text, const QString &label) {
        return text == label || text.startsWith(label + " (");
    };

    auto remove_child_prototypes = [&](const QString &label) {
        QStandardItem *group = nullptr;
        const int root_count = root->rowCount();
        for (int i = 0; i < root_count; ++i) {
            auto *child = root->child(i);
            if (!child) {
                continue;
            }
            if (matches_group(child->text(), label)) {
                group = child;
                break;
            }
        }

        if (!group) {
            return;
        }

        const int count = group->rowCount();
        for (int index = 0; index < count; ++index) {
            auto *item = group->child(index);
            if (!item) {
                continue;
            }
            if (item->data(kRoleType).toInt() != static_cast<int>(AssetNodeType::RenderObject)) {
                continue;
            }
            const QString name = item->data(kRoleName).toString();
            if (name.isEmpty()) {
                continue;
            }
            const QByteArray name_bytes = name.toLatin1();
            asset_manager->Remove_Prototype(name_bytes.constData());
        }
    };

    remove_child_prototypes("Mesh");
    remove_child_prototypes("Hierarchy");
    remove_child_prototypes("Mesh Collection");
}

void W3DViewMainWindow::reloadDisplayedObject()
{
    if (!_treeView) {
        return;
    }

    const QModelIndex current = _treeView->currentIndex();
    if (current.isValid()) {
        onCurrentChanged(current, QModelIndex());
    }
}

bool W3DViewMainWindow::loadAssetsFromFile(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        QMessageBox::warning(this, "W3DViewQt", QString("File not found:\n%1").arg(path));
        return false;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        QMessageBox::warning(this, "W3DViewQt", "WW3D asset manager is not available.");
        return false;
    }

    asset_manager->Load_Procedural_Textures();

    const QString directory = QFileInfo(path).absolutePath();
    if (!directory.isEmpty()) {
        QDir::setCurrent(directory);
        applyTexturePath(directory);
    }

    const QByteArray path_bytes = QDir::toNativeSeparators(path).toLocal8Bit();
    if (!asset_manager->Load_3D_Assets(path_bytes.constData())) {
        QMessageBox::warning(this, "W3DViewQt", "Failed to load W3D assets.");
        return false;
    }

    LoadMissingHierarchyAssets(asset_manager, directory);

    _lastOpenedPath = info.absolutePath();
    QSettings settings;
    settings.setValue("Config/LastOpenedPath", _lastOpenedPath);
    setWindowTitle(QString("W3DViewQt - %1").arg(info.fileName()));
    rebuildAssetTree();
    addRecentFile(info.absoluteFilePath());
    statusBar()->showMessage(QString("Loaded: %1").arg(info.fileName()));
    return true;
}

void W3DViewMainWindow::updateRecentFilesMenu()
{
    if (!_recentFilesMenu) {
        return;
    }

    _recentFilesMenu->clear();
    QSettings settings;
    const auto files = settings.value(QStringLiteral("recentFiles")).toStringList();
    if (files.isEmpty()) {
        auto *empty_action = _recentFilesMenu->addAction("(No recent files)");
        empty_action->setEnabled(false);
        return;
    }

    int index = 1;
    for (const QString &path : files) {
        const QFileInfo info(path);
        const QString label = QString("&%1 %2").arg(index++).arg(info.fileName());
        auto *action = _recentFilesMenu->addAction(label);
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, &W3DViewMainWindow::openRecentFile);
    }
}

void W3DViewMainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    QSettings settings;
    auto files = settings.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    settings.setValue(QStringLiteral("recentFiles"), files);
    updateRecentFilesMenu();
}

void W3DViewMainWindow::rebuildAssetTree()
{
    _treeModel->clear();
    _treeModel->setHorizontalHeaderLabels(QStringList() << "Assets");

    auto *root = _treeModel->invisibleRootItem();
    auto *materials_group = new QStandardItem("Materials");
    materials_group->setEditable(false);
    materials_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(materials_group);

    auto *mesh_group = new QStandardItem("Mesh");
    mesh_group->setEditable(false);
    mesh_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(mesh_group);

    auto *hierarchy_group = new QStandardItem("Hierarchy");
    hierarchy_group->setEditable(false);
    hierarchy_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(hierarchy_group);

    auto *hlod_group = new QStandardItem("H-LOD");
    hlod_group->setEditable(false);
    hlod_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(hlod_group);

    auto *collection_group = new QStandardItem("Mesh Collection");
    collection_group->setEditable(false);
    collection_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(collection_group);

    auto *aggregate_group = new QStandardItem("Aggregate");
    aggregate_group->setEditable(false);
    aggregate_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(aggregate_group);

    auto *emitter_group = new QStandardItem("Emitter");
    emitter_group->setEditable(false);
    emitter_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(emitter_group);

    auto *primitives_group = new QStandardItem("Primitives");
    primitives_group->setEditable(false);
    primitives_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(primitives_group);

    auto *sound_group = new QStandardItem("Sounds");
    sound_group->setEditable(false);
    sound_group->setData(static_cast<int>(AssetNodeType::Group), kRoleType);
    root->appendRow(sound_group);

    addMaterialItems(materials_group);
    addRenderObjectItems(mesh_group,
                         hierarchy_group,
                         hlod_group,
                         collection_group,
                         aggregate_group,
                         emitter_group,
                         primitives_group,
                         sound_group);
    addAnimationItems(hierarchy_group, hlod_group, aggregate_group);

    materials_group->sortChildren(0, Qt::AscendingOrder);
    mesh_group->sortChildren(0, Qt::AscendingOrder);
    hierarchy_group->sortChildren(0, Qt::AscendingOrder);
    hlod_group->sortChildren(0, Qt::AscendingOrder);
    collection_group->sortChildren(0, Qt::AscendingOrder);
    aggregate_group->sortChildren(0, Qt::AscendingOrder);
    emitter_group->sortChildren(0, Qt::AscendingOrder);
    primitives_group->sortChildren(0, Qt::AscendingOrder);
    sound_group->sortChildren(0, Qt::AscendingOrder);
    SortAnimationChildren(hierarchy_group);
    SortAnimationChildren(hlod_group);
    SortAnimationChildren(aggregate_group);

    _treeView->setExpanded(materials_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(mesh_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(hierarchy_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(hlod_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(collection_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(aggregate_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(emitter_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(primitives_group->index(), _autoExpandAssetTree);
    _treeView->setExpanded(sound_group->index(), _autoExpandAssetTree);
}

void W3DViewMainWindow::addMaterialItems(QStandardItem *parent)
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return;
    }

    int count = 0;
    HashTemplateIterator<StringClass, TextureClass *> iterator(asset_manager->Texture_Hash());
    for (iterator.First(); !iterator.Is_Done(); iterator.Next()) {
        const char *name = iterator.Peek_Key();
        if (!name || !name[0]) {
            continue;
        }

        auto *texture = iterator.Peek_Value();
        auto *item = new QStandardItem(QString::fromLatin1(name));
        item->setEditable(false);
        item->setData(static_cast<int>(AssetNodeType::Material), kRoleType);
        item->setData(QString::fromLatin1(name), kRoleName);
        item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(texture)), kRolePointer);
        parent->appendRow(item);
        ++count;
    }

    parent->setText(QString("Materials (%1)").arg(count));
}

void W3DViewMainWindow::addRenderObjectItems(QStandardItem *meshParent,
                                             QStandardItem *hierarchyParent,
                                             QStandardItem *hlodParent,
                                             QStandardItem *collectionParent,
                                             QStandardItem *aggregateParent,
                                             QStandardItem *emitterParent,
                                             QStandardItem *primitivesParent,
                                             QStandardItem *soundParent)
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return;
    }

    RenderObjIterator *iterator = asset_manager->Create_Render_Obj_Iterator();
    if (!iterator) {
        return;
    }

    int mesh_count = 0;
    int hierarchy_count = 0;
    int hlod_count = 0;
    int collection_count = 0;
    int aggregate_count = 0;
    int emitter_count = 0;
    int primitive_count = 0;
    int sound_count = 0;
    for (iterator->First(); !iterator->Is_Done(); iterator->Next()) {
        const char *name = iterator->Current_Item_Name();
        if (!name || !name[0]) {
            continue;
        }

        if (!asset_manager->Render_Obj_Exists(name)) {
            continue;
        }

        const int class_id = iterator->Current_Item_Class_ID();
        QStandardItem *parent = nullptr;
        bool insert = false;

        switch (class_id) {
        case RenderObjClass::CLASSID_COLLECTION:
            insert = true;
            parent = collectionParent;
            break;
        case RenderObjClass::CLASSID_MESH:
            insert = true;
            parent = meshParent;
            break;
        case RenderObjClass::CLASSID_SOUND:
            insert = true;
            parent = soundParent;
            break;
        case RenderObjClass::CLASSID_PARTICLEEMITTER:
            insert = true;
            parent = emitterParent;
            break;
        case RenderObjClass::CLASSID_SPHERE:
        case RenderObjClass::CLASSID_RING:
            insert = true;
            parent = primitivesParent;
            break;
        case RenderObjClass::CLASSID_DISTLOD:
        case RenderObjClass::CLASSID_HLOD:
            insert = true;
            parent = hierarchyParent;
            break;
        case RenderObjClass::CLASSID_HMODEL:
            insert = true;
            parent = hierarchyParent;
            break;
        default:
            break;
        }

        if (!insert || !parent) {
            continue;
        }

        const RenderObjInfo info = InspectRenderObj(name);
        if ((class_id == RenderObjClass::CLASSID_DISTLOD ||
             class_id == RenderObjClass::CLASSID_HLOD) &&
            info.isRealLod) {
            parent = hlodParent;
        }

        if (info.isAggregate) {
            parent = aggregateParent;
        }

        auto *item = new QStandardItem(QString::fromLatin1(name));
        item->setEditable(false);
        item->setData(static_cast<int>(AssetNodeType::RenderObject), kRoleType);
        item->setData(QString::fromLatin1(name), kRoleName);
        item->setData(class_id, kRoleClassId);
        if (!info.hierarchyName.isEmpty()) {
            item->setData(info.hierarchyName, kRoleHierarchyName);
        }
        parent->appendRow(item);

        if (parent == meshParent) {
            ++mesh_count;
        } else if (parent == hierarchyParent) {
            ++hierarchy_count;
        } else if (parent == hlodParent) {
            ++hlod_count;
        } else if (parent == collectionParent) {
            ++collection_count;
        } else if (parent == aggregateParent) {
            ++aggregate_count;
        } else if (parent == emitterParent) {
            ++emitter_count;
        } else if (parent == primitivesParent) {
            ++primitive_count;
        } else if (parent == soundParent) {
            ++sound_count;
        }
    }

    asset_manager->Release_Render_Obj_Iterator(iterator);
    meshParent->setText(QString("Mesh (%1)").arg(mesh_count));
    hierarchyParent->setText(QString("Hierarchy (%1)").arg(hierarchy_count));
    hlodParent->setText(QString("H-LOD (%1)").arg(hlod_count));
    collectionParent->setText(QString("Mesh Collection (%1)").arg(collection_count));
    aggregateParent->setText(QString("Aggregate (%1)").arg(aggregate_count));
    emitterParent->setText(QString("Emitter (%1)").arg(emitter_count));
    primitivesParent->setText(QString("Primitives (%1)").arg(primitive_count));
    soundParent->setText(QString("Sounds (%1)").arg(sound_count));
}

void W3DViewMainWindow::addAnimationItems(QStandardItem *hierarchyParent,
                                          QStandardItem *hlodParent,
                                          QStandardItem *aggregateParent)
{
    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return;
    }

    AssetIterator *iterator = asset_manager->Create_HAnim_Iterator();
    if (!iterator) {
        return;
    }

    for (iterator->First(); !iterator->Is_Done(); iterator->Next()) {
        const char *anim_name = iterator->Current_Item_Name();
        if (!anim_name || !anim_name[0]) {
            continue;
        }

        HAnimClass *anim = asset_manager->Get_HAnim(anim_name);
        if (!anim) {
            continue;
        }

        const char *hier_name = anim->Get_HName();
        const QString hierarchy = hier_name ? QString::fromLatin1(hier_name) : QString();
        anim->Release_Ref();

        if (hierarchy.isEmpty()) {
            continue;
        }

        QVector<QStandardItem *> targets;
        if (_restrictAnims) {
            CollectHierarchyItems(hierarchyParent, hierarchy, targets);
            CollectHierarchyItems(hlodParent, hierarchy, targets);
            CollectHierarchyItems(aggregateParent, hierarchy, targets);
        } else {
            CollectAllChildren(hierarchyParent, targets);
            CollectAllChildren(hlodParent, targets);
            CollectAllChildren(aggregateParent, targets);
        }

        if (targets.isEmpty()) {
            continue;
        }

        const QString anim_text = QString::fromLatin1(anim_name);
        for (auto *target : targets) {
            if (!target) {
                continue;
            }

            auto *item = new QStandardItem(anim_text);
            item->setEditable(false);
            item->setData(static_cast<int>(AssetNodeType::Animation), kRoleType);
            item->setData(anim_text, kRoleName);
            target->appendRow(item);
        }
    }

    delete iterator;
}

void W3DViewMainWindow::loadAppSettings()
{
    QSettings settings;
    _lastOpenedPath = settings.value("Config/LastOpenedPath").toString();
    _texturePath1 = settings.value("Config/TexturePath1").toString();
    _texturePath2 = settings.value("Config/TexturePath2").toString();
    _sortingEnabled = settings.value("Config/EnableSorting", true).toBool();
    _animateCamera = settings.value("Config/AnimateCamera", false).toBool();
    _autoResetCamera = settings.value("Config/ResetCamera", true).toBool();
    _autoExpandAssetTree = settings.value("Config/AutoExpandAssetTree", true).toBool();
    const bool invert_culling = settings.value("Config/InvertBackfaceCulling", false).toBool();
    const bool manual_fov = settings.value("Config/UseManualFOV", false).toBool();
    const bool manual_clip = settings.value("Config/UseManualClipPlanes", false).toBool();
    const double hfov_rad = settings.value("Config/hfov", 0.0).toDouble();
    const double vfov_rad = settings.value("Config/vfov", 0.0).toDouble();
    const float znear = settings.value("Config/znear", 0.2).toFloat();
    const float zfar = settings.value("Config/zfar", 10000.0).toFloat();
    const bool gamma_enabled = settings.value("Config/EnableGamma", 0).toInt() != 0;
    const bool munge_sort = settings.value("Config/MungeSortOnLoad", 0).toInt() != 0;
    int npatches_level = settings.value("Config/NPatchesSubdivision", 4).toInt();
    if (npatches_level < 1) {
        npatches_level = 1;
    }
    if (npatches_level > 8) {
        npatches_level = 8;
    }
    const bool npatches_gap = settings.value("Config/NPatchesGapFilling", 0).toInt() != 0;
    WW3D::Enable_Sorting(_sortingEnabled);

    applyTexturePath(_texturePath1);
    applyTexturePath(_texturePath2);

    if (_viewport) {
        _viewport->setCameraAnimationEnabled(_animateCamera);
        _viewport->setAutoResetEnabled(_autoResetCamera);
        _viewport->setManualFovEnabled(manual_fov);
        if (manual_fov && hfov_rad > 0.0 && vfov_rad > 0.0) {
            _viewport->setCameraFovDegrees(hfov_rad * kRadToDeg, vfov_rad * kRadToDeg);
        }
        _viewport->setManualClipPlanesEnabled(manual_clip);
        if (manual_clip) {
            _viewport->setCameraClipPlanes(znear, zfar);
        }
    }

    if (_enableGammaAction) {
        _enableGammaAction->setChecked(gamma_enabled);
    }
    if (_mungeSortAction) {
        _mungeSortAction->setChecked(munge_sort);
    }
    if (_autoExpandTreeAction) {
        _autoExpandTreeAction->setChecked(_autoExpandAssetTree);
    }
    if (gamma_enabled) {
        int gamma = settings.value("Config/Gamma", 10).toInt();
        if (gamma < 10) {
            gamma = 10;
        }
        if (gamma > 30) {
            gamma = 30;
        }
        DX8Wrapper::Set_Gamma(gamma / 10.0f, 0.0f, 1.0f);
    }

    ShaderClass::Invert_Backface_Culling(invert_culling);
    WW3D::Enable_Munge_Sort_On_Load(munge_sort);

    WW3D::Set_NPatches_Level(static_cast<unsigned int>(npatches_level));
    WW3D::Set_NPatches_Gap_Filling_Mode(
        npatches_gap ? WW3D::NPATCHES_GAP_FILLING_ENABLED
                     : WW3D::NPATCHES_GAP_FILLING_DISABLED);
    if (_npatchesGroup) {
        for (auto *action : _npatchesGroup->actions()) {
            if (action && action->data().toInt() == npatches_level) {
                action->setChecked(true);
                break;
            }
        }
    }
    if (_npatchesGapAction) {
        _npatchesGapAction->setChecked(npatches_gap);
    }
}

void W3DViewMainWindow::loadDefaultSettings()
{
    const QString default_path = QDir(QCoreApplication::applicationDirPath()).filePath("default.dat");
    if (!QFileInfo::exists(default_path)) {
        return;
    }

    QSettings settings(default_path, QSettings::IniFormat);
    applySettings(settings);
}

void W3DViewMainWindow::applyTexturePath(const QString &path)
{
    const QString trimmed = QDir::cleanPath(path.trimmed());
    if (trimmed.isEmpty() || !_TheSimpleFileFactory) {
        return;
    }

    const QByteArray native = QDir::toNativeSeparators(trimmed).toLocal8Bit();
    _TheSimpleFileFactory->Append_Sub_Directory(native.constData());
}

void W3DViewMainWindow::setTexturePaths(const QString &path1, const QString &path2)
{
    QSettings settings;

    const QString cleaned1 = QDir::cleanPath(path1.trimmed());
    if (cleaned1.compare(_texturePath1, Qt::CaseInsensitive) != 0) {
        applyTexturePath(cleaned1);
        _texturePath1 = cleaned1;
        settings.setValue("Config/TexturePath1", _texturePath1);
    }

    const QString cleaned2 = QDir::cleanPath(path2.trimmed());
    if (cleaned2.compare(_texturePath2, Qt::CaseInsensitive) != 0) {
        applyTexturePath(cleaned2);
        _texturePath2 = cleaned2;
        settings.setValue("Config/TexturePath2", _texturePath2);
    }
}

void W3DViewMainWindow::applySettings(QSettings &settings)
{
    if (!_viewport) {
        return;
    }

    settings.beginGroup("Settings");

    if (settings.contains("AmbientLightR") && settings.contains("AmbientLightG") &&
        settings.contains("AmbientLightB")) {
        const float amb_r = settings.value("AmbientLightR").toFloat();
        const float amb_g = settings.value("AmbientLightG").toFloat();
        const float amb_b = settings.value("AmbientLightB").toFloat();
        _viewport->setAmbientLight(Vector3(amb_r, amb_g, amb_b));
    }

    if (settings.contains("SceneLightR") && settings.contains("SceneLightG") &&
        settings.contains("SceneLightB")) {
        const float light_r = settings.value("SceneLightR").toFloat();
        const float light_g = settings.value("SceneLightG").toFloat();
        const float light_b = settings.value("SceneLightB").toFloat();
        _viewport->setSceneLightColor(Vector3(light_r, light_g, light_b));
    }

    if (settings.contains("SceneLightX") && settings.contains("SceneLightY") &&
        settings.contains("SceneLightZ") && settings.contains("SceneLightW")) {
        Quaternion orientation(true);
        orientation.X = settings.value("SceneLightX").toFloat();
        orientation.Y = settings.value("SceneLightY").toFloat();
        orientation.Z = settings.value("SceneLightZ").toFloat();
        orientation.W = settings.value("SceneLightW").toFloat();
        _viewport->setSceneLightOrientation(orientation);
    }

    if (settings.contains("SceneLightDistance") && settings.contains("SceneLightIntensity") &&
        settings.contains("SceneLightAttenStart") && settings.contains("SceneLightAttenEnd") &&
        settings.contains("SceneLightAttenOn")) {
        const float distance = settings.value("SceneLightDistance").toFloat();
        const float intensity = settings.value("SceneLightIntensity").toFloat();
        const float atten_start = settings.value("SceneLightAttenStart").toFloat();
        const float atten_end = settings.value("SceneLightAttenEnd").toFloat();
        const bool atten_on = settings.value("SceneLightAttenOn").toBool();
        _viewport->setSceneLightIntensity(intensity);
        _viewport->setSceneLightAttenuation(atten_start, atten_end, atten_on);
        _viewport->setSceneLightDistance(distance);
    }

    if (settings.contains("BackgroundR") && settings.contains("BackgroundG") &&
        settings.contains("BackgroundB")) {
        const float bg_r = settings.value("BackgroundR").toFloat();
        const float bg_g = settings.value("BackgroundG").toFloat();
        const float bg_b = settings.value("BackgroundB").toFloat();
        _viewport->setBackgroundColor(Vector3(bg_r, bg_g, bg_b));
    }

    if (settings.contains("BackgroundBMP")) {
        _viewport->setBackgroundBitmap(settings.value("BackgroundBMP").toString());
    }

    if (settings.contains("FogEnabled")) {
        _viewport->setFogEnabled(settings.value("FogEnabled").toBool());
    }

    settings.endGroup();
    if (_fogAction) {
        _fogAction->setChecked(_viewport->isFogEnabled());
    }
}

void W3DViewMainWindow::writeSettings(QSettings &settings) const
{
    if (!_viewport) {
        return;
    }

    settings.beginGroup("Settings");

    const Vector3 ambient = _viewport->ambientLight();
    settings.setValue("AmbientLightR", ambient.X);
    settings.setValue("AmbientLightG", ambient.Y);
    settings.setValue("AmbientLightB", ambient.Z);

    const Vector3 scene_light = _viewport->sceneLightColor();
    settings.setValue("SceneLightR", scene_light.X);
    settings.setValue("SceneLightG", scene_light.Y);
    settings.setValue("SceneLightB", scene_light.Z);

    const Quaternion orientation = _viewport->sceneLightOrientation();
    settings.setValue("SceneLightX", orientation.X);
    settings.setValue("SceneLightY", orientation.Y);
    settings.setValue("SceneLightZ", orientation.Z);
    settings.setValue("SceneLightW", orientation.W);

    settings.setValue("SceneLightDistance", _viewport->sceneLightDistance());
    settings.setValue("SceneLightIntensity", _viewport->sceneLightIntensity());

    float atten_start = 0.0f;
    float atten_end = 0.0f;
    bool atten_on = false;
    _viewport->sceneLightAttenuation(atten_start, atten_end, atten_on);
    settings.setValue("SceneLightAttenStart", atten_start);
    settings.setValue("SceneLightAttenEnd", atten_end);
    settings.setValue("SceneLightAttenOn", atten_on ? 1 : 0);

    const Vector3 background = _viewport->backgroundColor();
    settings.setValue("BackgroundR", background.X);
    settings.setValue("BackgroundG", background.Y);
    settings.setValue("BackgroundB", background.Z);
    settings.setValue("BackgroundBMP", _viewport->backgroundBitmap());
    settings.setValue("FogEnabled", _viewport->isFogEnabled());

    settings.endGroup();
}
