#include "PresetsPanel.h"

#include "definitionfactory.h"
#include "definitionfactorymgr.h"
#include "combatchunkid.h"
#include "EditorChunkIDs.h"

#include <algorithm>
#include <functional>

#include <QAbstractItemView>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>

namespace leveledit_qt {

namespace {

constexpr std::uint32_t kDefClassStart = 0x00001000;
constexpr std::uint32_t kDefClassRange = 0x00001000;
constexpr std::uint32_t kClassTerrain = 0x00001000;
constexpr std::uint32_t kClassTile = 0x00002000;
constexpr std::uint32_t kClassGameObjects = 0x00003000;
constexpr std::uint32_t kClassLight = 0x00004000;
constexpr std::uint32_t kClassSound = 0x00005000;
constexpr std::uint32_t kClassWaypath = 0x00006000;
constexpr std::uint32_t kClassEditorObjects = 0x0000A000;
constexpr std::uint32_t kClassMunitions = 0x0000B000;
constexpr std::uint32_t kClassDummyObjects = 0x0000C000;
constexpr std::uint32_t kClassBuildings = 0x0000D000;
constexpr std::uint32_t kClassTwiddlers = 0x0000E000;
constexpr std::uint32_t kClassGlobalSettings = 0x0000F000;
constexpr std::uint32_t kClassCoverSpots = 0x0000A005;
constexpr std::uint32_t kCategoryUnknown = 0;
constexpr int kPresetRowRole = Qt::UserRole + 1;

struct CategoryEntry
{
    std::uint32_t id = 0;
    const char *name = "";
};

constexpr CategoryEntry kPresetCategories[] = {
    {kClassTerrain, "Terrain"},
    {kClassTile, "Tile"},
    {kClassGameObjects, "Object"},
    {kClassBuildings, "Buildings"},
    {kClassMunitions, "Munitions"},
    {kClassDummyObjects, "Dummy Object"},
    {kClassCoverSpots, "Cover Spots"},
    {kClassLight, "Light"},
    {kClassSound, "Sound"},
    {kClassWaypath, "Waypath"},
    {kClassTwiddlers, "Twiddlers"},
    {kClassEditorObjects, "Editor Objects"},
    {kClassGlobalSettings, "Global Settings"},
};
constexpr int kPresetCategoryCount = static_cast<int>(sizeof(kPresetCategories) / sizeof(kPresetCategories[0]));

std::uint32_t SuperClassIdFromClassId(std::uint32_t class_id)
{
    if (class_id < kDefClassStart) {
        return class_id;
    }

    const std::uint32_t delta = class_id - kDefClassStart;
    return kDefClassStart + ((delta / kDefClassRange) * kDefClassRange);
}

std::uint32_t CategoryIdForClassId(std::uint32_t class_id)
{
    if (class_id == kClassCoverSpots) {
        return kClassCoverSpots;
    }

    switch (SuperClassIdFromClassId(class_id)) {
    case kClassTerrain:
        return kClassTerrain;
    case kClassTile:
        return kClassTile;
    case kClassGameObjects:
        return kClassGameObjects;
    case kClassBuildings:
        return kClassBuildings;
    case kClassMunitions:
        return kClassMunitions;
    case kClassDummyObjects:
        return kClassDummyObjects;
    case kClassLight:
        return kClassLight;
    case kClassSound:
        return kClassSound;
    case kClassWaypath:
        return kClassWaypath;
    case kClassTwiddlers:
        return kClassTwiddlers;
    case kClassEditorObjects:
        return kClassEditorObjects;
    case kClassGlobalSettings:
        return kClassGlobalSettings;
    default:
        return kCategoryUnknown;
    }
}

QString CategoryNameForId(std::uint32_t category_id)
{
    for (const CategoryEntry &entry : kPresetCategories) {
        if (entry.id == category_id) {
            return QString::fromLatin1(entry.name);
        }
    }

    if (category_id == kCategoryUnknown) {
        return QStringLiteral("Other");
    }

    return QStringLiteral("Class %1").arg(category_id);
}

DefinitionFactoryClass *FindDisplayedFactory(std::uint32_t class_id)
{
    DefinitionFactoryClass *factory = DefinitionFactoryMgrClass::Find_Factory(class_id);
    if (factory == nullptr || !factory->Is_Displayed()) {
        return nullptr;
    }
    return factory;
}

QString FactoryFolderNameForClassId(std::uint32_t class_id)
{
    if (DefinitionFactoryClass *factory = FindDisplayedFactory(class_id)) {
        const char *name = factory->Get_Name();
        if (name != nullptr && name[0] != '\0') {
            return QString::fromLatin1(name);
        }
    }

    switch (class_id) {
    case CLASSID_TERRAIN:
        return QStringLiteral("Terrain");
    case CLASSID_TILE:
        return QStringLiteral("Tile");
    case CLASSID_GAME_OBJECTS:
        return QStringLiteral("Object");
    case CLASSID_LIGHT:
        return QStringLiteral("Light");
    case CLASSID_SOUND:
        return QStringLiteral("Sound");
    case CLASSID_WAYPATH:
        return QStringLiteral("Waypath");
    case CLASSID_MUNITIONS:
        return QStringLiteral("Munitions");
    case CLASSID_DUMMY_OBJECTS:
        return QStringLiteral("Dummy Object");
    case CLASSID_BUILDINGS:
        return QStringLiteral("Buildings");
    case CLASSID_TWIDDLERS:
        return QStringLiteral("Twiddlers");
    case CLASSID_EDITOR_OBJECTS:
        return QStringLiteral("Editor Objects");
    case CLASSID_GLOBAL_SETTINGS:
        return QStringLiteral("Global Settings");
    case CLASSID_GAME_OBJECT_DEF_SOLDIER:
        return QStringLiteral("Soldier");
    case CLASSID_GAME_OBJECT_DEF_POWERUP:
        return QStringLiteral("PowerUp");
    case CLASSID_GAME_OBJECT_DEF_SIMPLE:
        return QStringLiteral("Simple");
    case CLASSID_GAME_OBJECT_DEF_C4:
        return QStringLiteral("C4");
    case CLASSID_GAME_OBJECT_DEF_SAMSITE:
        return QStringLiteral("SAMSite");
    case CLASSID_SPAWNER_DEF:
        return QStringLiteral("Spawner");
    case CLASSID_GAME_OBJECT_DEF_SCRIPT_ZONE:
        return QStringLiteral("Script Zone");
    case CLASSID_GAME_OBJECT_DEF_TRANSITION:
        return QStringLiteral("Transition");
    case CLASSID_GAME_OBJECT_DEF_VEHICLE:
        return QStringLiteral("Vehicle");
    case CLASSID_GAME_OBJECT_DEF_CINEMATIC:
        return QStringLiteral("Cinematic");
    case CLASSID_GAME_OBJECT_DEF_DAMAGE_ZONE:
        return QStringLiteral("Damage Zone");
    case CLASSID_GAME_OBJECT_DEF_SPECIAL_EFFECTS:
        return QStringLiteral("Special Effects");
    case CLASSID_GAME_OBJECT_DEF_SAKURA_BOSS:
        return QStringLiteral("Sakura Boss");
    case CLASSID_GAME_OBJECT_DEF_BEACON:
        return QStringLiteral("Beacon");
    case CLASSID_GAME_OBJECT_DEF_MENDOZA_BOSS:
        return QStringLiteral("Mendoza Boss");
    case CLASSID_GAME_OBJECT_DEF_RAVESHAW_BOSS:
        return QStringLiteral("Raveshaw Boss");
    case CLASSID_GAME_OBJECT_DEF_BUILDING:
        return QStringLiteral("<Generic Building>");
    case CLASSID_GAME_OBJECT_DEF_REFINERY:
        return QStringLiteral("Refinery");
    case CLASSID_GAME_OBJECT_DEF_POWERPLANT:
        return QStringLiteral("Powerplant");
    case CLASSID_GAME_OBJECT_DEF_SOLDIER_FACTORY:
        return QStringLiteral("Soldier Factory");
    case CLASSID_GAME_OBJECT_DEF_VEHICLE_FACTORY:
        return QStringLiteral("Vehicle Factory");
    case CLASSID_GAME_OBJECT_DEF_AIRSTRIP:
        return QStringLiteral("Airstrip");
    case CLASSID_GAME_OBJECT_DEF_WARFACTORY:
        return QStringLiteral("WarFactory");
    case CLASSID_GAME_OBJECT_DEF_COMCENTER:
        return QStringLiteral("Com Center");
    case CLASSID_GAME_OBJECT_DEF_REPAIR_BAY:
        return QStringLiteral("Repair Bay");
    case CLASSID_DEF_WEAPON:
        return QStringLiteral("Weapon");
    case CLASSID_DEF_AMMO:
        return QStringLiteral("Ammo");
    case CLASSID_DEF_EXPLOSION:
        return QStringLiteral("Explosion");
    case CLASSID_GLOBAL_SETTINGS_DEF_GENERAL:
        return QStringLiteral("General");
    case CLASSID_GLOBAL_SETTINGS_DEF_HUMAN_LOITER:
        return QStringLiteral("HumanLoiter");
    case CLASSID_GLOBAL_SETTINGS_DEF_HUD:
        return QStringLiteral("HUD");
    case CLASSID_GLOBAL_SETTINGS_DEF_EVA:
        return QStringLiteral("Eva Settings");
    case CLASSID_GLOBAL_SETTINGS_DEF_CHAR_CLASS:
        return QStringLiteral("Character Classes");
    case CLASSID_GLOBAL_SETTINGS_DEF_HUMAN_ANIM_OVERRIDE:
        return QStringLiteral("HUMAN_ANIM_OVERRIDE");
    case CLASSID_GLOBAL_SETTINGS_DEF_PURCHASE:
        return QStringLiteral("Purchase Settings");
    case CLASSID_GLOBAL_SETTINGS_DEF_TEAM_PURCHASE:
        return QStringLiteral("Team Purchase Settings");
    case CLASSID_GLOBAL_SETTINGS_DEF_CNCMODE:
        return QStringLiteral("C&C Mode Settings");
    case CLASSID_VIS_POINT_DEF:
        return QStringLiteral("Manual Vis Point");
    case CLASSID_PATHFIND_START_DEF:
        return QStringLiteral("Pathfind Generator");
    case CLASSID_LIGHT_DEF:
        return QStringLiteral("Light");
    case CLASSID_COVERSPOT:
        return QStringLiteral("Cover Spot");
    case CLASSID_EDITOR_ONLY_OBJECTS:
        return QStringLiteral("Editor Only Objects");
    default:
        break;
    }

    return QStringLiteral("Class %1").arg(class_id);
}

bool RowLess(const PresetsPanel::RowData &lhs, const PresetsPanel::RowData &rhs)
{
    const int name_compare = QString::compare(lhs.name, rhs.name, Qt::CaseInsensitive);
    if (name_compare != 0) {
        return name_compare < 0;
    }

    return lhs.id < rhs.id;
}

void SetPresetColumns(QTreeWidgetItem *item, const PresetsPanel::RowData &row)
{
    item->setText(0, row.name);
    item->setText(1, QString::number(row.id));
    item->setText(2, QString::number(row.class_id));
    item->setText(3, QString::number(row.parent_id));
    item->setText(4, row.temporary ? QStringLiteral("Yes") : QStringLiteral("No"));
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);

    item->setToolTip(0,
                     QStringLiteral("ID=%1  Class=%2  Parent=%3  Temp=%4")
                         .arg(row.id)
                         .arg(row.class_id)
                         .arg(row.parent_id)
                         .arg(row.temporary ? QStringLiteral("true")
                                            : QStringLiteral("false")));
}

bool RowMatchesFilter(const PresetsPanel::RowData &row, const QString &filter_text)
{
    if (filter_text.isEmpty()) {
        return true;
    }

    return row.name.contains(filter_text, Qt::CaseInsensitive) ||
        QString::number(row.id).contains(filter_text, Qt::CaseInsensitive) ||
        QString::number(row.class_id).contains(filter_text, Qt::CaseInsensitive) ||
        QString::number(row.parent_id).contains(filter_text, Qt::CaseInsensitive);
}

} // namespace

PresetsPanel::PresetsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    _filter = new QLineEdit(this);
    _filter->setPlaceholderText(QStringLiteral("Filter presets by name, id, class, or parent..."));

    _summary = new QLabel(QStringLiteral("Preset catalog not loaded."), this);
    _summary->setWordWrap(true);

    _tree = new QTreeWidget(this);
    _tree->setColumnCount(5);
    _tree->setHeaderLabels({
        QStringLiteral("Name"),
        QStringLiteral("ID"),
        QStringLiteral("Class"),
        QStringLiteral("Parent"),
        QStringLiteral("Temp"),
    });
    _tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    _tree->setSelectionMode(QAbstractItemView::SingleSelection);
    _tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _tree->setAlternatingRowColors(true);
    _tree->setRootIsDecorated(true);
    _tree->setUniformRowHeights(true);
    _tree->setSortingEnabled(false);
    _tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    connect(_filter, &QLineEdit::textChanged, this, [this]() { rebuildTree(); });
    connect(_tree,
            &QTreeWidget::currentItemChanged,
            this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                emitPresetActivatedForItem(current);
                if (_modButton != nullptr) {
                    _modButton->setEnabled(isPresetRowItem(current));
                }
            });
    connect(_tree,
            &QTreeWidget::itemDoubleClicked,
            this,
            [this](QTreeWidgetItem *item, int) {
                emitPresetOpenForItem(item);
            });

    _modButton = new QPushButton(QStringLiteral("Mod..."), this);
    _modButton->setEnabled(false);
    connect(_modButton, &QPushButton::clicked, this, [this]() {
        emitPresetOpenForItem(_tree ? _tree->currentItem() : nullptr);
    });

    auto *button_layout = new QHBoxLayout();
    button_layout->setContentsMargins(0, 0, 0, 0);
    button_layout->addStretch(1);
    button_layout->addWidget(_modButton);

    layout->addWidget(_filter);
    layout->addWidget(_summary);
    layout->addWidget(_tree, 1);
    layout->addLayout(button_layout);
}

void PresetsPanel::setPresets(const std::vector<PresetRecord> &records,
                              const QString &source,
                              const QString &error)
{
    _rows.clear();
    _rows.reserve(records.size());

    for (const PresetRecord &record : records) {
        RowData row;
        row.id = record.id;
        row.class_id = record.class_id;
        row.parent_id = record.parent_id;
        row.temporary = record.temporary;
        row.name = QString::fromStdString(record.name);

        _rows.push_back(row);
    }

    if (!error.isEmpty()) {
        _baseSummaryText = QStringLiteral("Presets unavailable: %1").arg(error);
    } else if (_rows.empty()) {
        _baseSummaryText = QStringLiteral("Preset catalog loaded, but no preset entries were found.");
    } else if (!source.isEmpty()) {
        _baseSummaryText = QStringLiteral("Loaded %1 presets from %2")
                               .arg(static_cast<qlonglong>(_rows.size()))
                               .arg(QDir::toNativeSeparators(source));
    } else {
        _baseSummaryText =
            QStringLiteral("Loaded %1 presets.").arg(static_cast<qlonglong>(_rows.size()));
    }

    rebuildTree();
}

void PresetsPanel::rebuildTree()
{
    if (_tree == nullptr || _filter == nullptr || _summary == nullptr) {
        return;
    }

    const QString filter_text = _filter->text().trimmed();

    QHash<std::uint32_t, int> id_to_row_all;
    id_to_row_all.reserve(static_cast<int>(_rows.size() * 2));
    for (int index = 0; index < static_cast<int>(_rows.size()); ++index) {
        if (_rows[static_cast<std::size_t>(index)].id != 0 &&
            !id_to_row_all.contains(_rows[static_cast<std::size_t>(index)].id)) {
            id_to_row_all.insert(_rows[static_cast<std::size_t>(index)].id, index);
        }
    }

    QVector<char> include_mask(static_cast<int>(_rows.size()), filter_text.isEmpty() ? 1 : 0);
    if (!filter_text.isEmpty()) {
        for (int index = 0; index < static_cast<int>(_rows.size()); ++index) {
            if (!RowMatchesFilter(_rows[static_cast<std::size_t>(index)], filter_text)) {
                continue;
            }

            include_mask[index] = 1;
            std::uint32_t parent_id = _rows[static_cast<std::size_t>(index)].parent_id;
            QSet<std::uint32_t> guard;
            while (parent_id != 0 && !guard.contains(parent_id)) {
                guard.insert(parent_id);
                const auto parent_it = id_to_row_all.constFind(parent_id);
                if (parent_it == id_to_row_all.cend()) {
                    break;
                }

                include_mask[parent_it.value()] = 1;
                parent_id = _rows[static_cast<std::size_t>(parent_it.value())].parent_id;
            }
        }
    }

    int shown_count = 0;
    for (char include : include_mask) {
        if (include != 0) {
            ++shown_count;
        }
    }

    _tree->setUpdatesEnabled(false);
    _tree->clear();

    if (shown_count == 0) {
        _summary->setText(_baseSummaryText);
        if (_modButton != nullptr) {
            _modButton->setEnabled(false);
        }
        _tree->setUpdatesEnabled(true);
        return;
    }

    QHash<std::uint32_t, QVector<int>> rows_by_class_id;
    rows_by_class_id.reserve(static_cast<int>(_rows.size()));
    for (int index = 0; index < static_cast<int>(_rows.size()); ++index) {
        if (include_mask[index] != 0) {
            const RowData &row = _rows[static_cast<std::size_t>(index)];
            rows_by_class_id[row.class_id].push_back(index);
        }
    }

    auto sort_indices = [this](QVector<int> &indices) {
        std::sort(indices.begin(), indices.end(), [this](int lhs, int rhs) {
            return RowLess(_rows[static_cast<std::size_t>(lhs)], _rows[static_cast<std::size_t>(rhs)]);
        });
    };

    auto add_class_tree = [&](QTreeWidgetItem *parent_item, const QVector<int> &class_rows) {
        if (parent_item == nullptr || class_rows.isEmpty()) {
            return;
        }

        QHash<std::uint32_t, int> id_to_row_shown;
        id_to_row_shown.reserve(class_rows.size() * 2);
        for (int row_index : class_rows) {
            const RowData &row = _rows[static_cast<std::size_t>(row_index)];
            if (row.id != 0 && !id_to_row_shown.contains(row.id)) {
                id_to_row_shown.insert(row.id, row_index);
            }
        }

        QHash<std::uint32_t, QVector<int>> children_by_parent_id;
        QVector<int> root_rows;
        root_rows.reserve(class_rows.size());

        for (int row_index : class_rows) {
            const RowData &row = _rows[static_cast<std::size_t>(row_index)];
            bool linked_to_parent = false;

            if (row.parent_id != 0) {
                const auto parent_it = id_to_row_shown.constFind(row.parent_id);
                if (parent_it != id_to_row_shown.cend() && parent_it.value() != row_index) {
                    const RowData &parent_row = _rows[static_cast<std::size_t>(parent_it.value())];
                    if (parent_row.class_id == row.class_id) {
                        children_by_parent_id[row.parent_id].push_back(row_index);
                        linked_to_parent = true;
                    }
                }
            }

            if (!linked_to_parent) {
                root_rows.push_back(row_index);
            }
        }

        for (auto it = children_by_parent_id.begin(); it != children_by_parent_id.end(); ++it) {
            sort_indices(it.value());
        }
        sort_indices(root_rows);

        std::function<void(QTreeWidgetItem *, int, QSet<std::uint32_t> &)> add_preset_item;
        add_preset_item = [&](QTreeWidgetItem *parent, int row_index, QSet<std::uint32_t> &stack_guard) {
            const RowData &row = _rows[static_cast<std::size_t>(row_index)];
            auto *preset_item = new QTreeWidgetItem(parent);
            SetPresetColumns(preset_item, row);
            preset_item->setData(0, kPresetRowRole, row_index);

            if (row.id == 0 || stack_guard.contains(row.id)) {
                return;
            }

            stack_guard.insert(row.id);
            const QVector<int> child_indices = children_by_parent_id.value(row.id);
            for (int child_row : child_indices) {
                add_preset_item(preset_item, child_row, stack_guard);
            }
            stack_guard.remove(row.id);
        };

        QSet<std::uint32_t> stack_guard;
        for (int row_index : root_rows) {
            add_preset_item(parent_item, row_index, stack_guard);
        }
    };

    auto rows_for_category = [&](std::uint32_t category_id) {
        int count = 0;
        for (auto it = rows_by_class_id.constBegin(); it != rows_by_class_id.constEnd(); ++it) {
            if (CategoryIdForClassId(it.key()) == category_id) {
                count += it.value().size();
            }
        }
        return count;
    };

    QVector<std::uint32_t> ordered_category_ids;
    ordered_category_ids.reserve(kPresetCategoryCount);
    QSet<std::uint32_t> seen_categories;
    for (const CategoryEntry &entry : kPresetCategories) {
        if (rows_for_category(entry.id) > 0) {
            ordered_category_ids.push_back(entry.id);
            seen_categories.insert(entry.id);
        }
    }

    QVector<std::uint32_t> extra_category_ids;
    for (auto it = rows_by_class_id.constBegin(); it != rows_by_class_id.constEnd(); ++it) {
        const std::uint32_t category_id = CategoryIdForClassId(it.key());
        if (!seen_categories.contains(category_id) && rows_for_category(category_id) > 0) {
            extra_category_ids.push_back(category_id);
            seen_categories.insert(category_id);
        }
    }
    std::sort(extra_category_ids.begin(), extra_category_ids.end());
    for (std::uint32_t category_id : extra_category_ids) {
        ordered_category_ids.push_back(category_id);
    }

    for (std::uint32_t category_id : ordered_category_ids) {
        const int category_row_count = rows_for_category(category_id);
        if (category_row_count <= 0) {
            continue;
        }

        auto *category_item = new QTreeWidgetItem(_tree);
        category_item->setText(0, CategoryNameForId(category_id));
        category_item->setText(1, QString::number(category_row_count));

        QFont category_font = category_item->font(0);
        category_font.setBold(true);
        category_item->setFont(0, category_font);

        QSet<std::uint32_t> consumed_class_ids;
        DefinitionFactoryClass *direct_factory = FindDisplayedFactory(category_id);
        if (direct_factory != nullptr) {
            const QVector<int> class_rows = rows_by_class_id.value(category_id);
            add_class_tree(category_item, class_rows);
            consumed_class_ids.insert(category_id);
        } else {
            for (DefinitionFactoryClass *factory = DefinitionFactoryMgrClass::Get_First(category_id);
                 factory != nullptr;
                 factory = DefinitionFactoryMgrClass::Get_Next(factory, category_id)) {
                if (!factory->Is_Displayed()) {
                    continue;
                }

                const std::uint32_t class_id = factory->Get_Class_ID();
                const QVector<int> class_rows = rows_by_class_id.value(class_id);
                if (class_rows.isEmpty()) {
                    continue;
                }

                consumed_class_ids.insert(class_id);

                auto *class_item = new QTreeWidgetItem(category_item);
                const char *factory_name = factory->Get_Name();
                class_item->setText(0,
                                    (factory_name != nullptr && factory_name[0] != '\0')
                                        ? QString::fromLatin1(factory_name)
                                        : QStringLiteral("Class %1").arg(class_id));
                class_item->setText(1, QString::number(class_rows.size()));
                add_class_tree(class_item, class_rows);
                class_item->setExpanded(true);
            }
        }

        QVector<std::uint32_t> fallback_class_ids;
        for (auto it = rows_by_class_id.constBegin(); it != rows_by_class_id.constEnd(); ++it) {
            if (CategoryIdForClassId(it.key()) != category_id) {
                continue;
            }

            if (!consumed_class_ids.contains(it.key())) {
                fallback_class_ids.push_back(it.key());
            }
        }
        std::sort(fallback_class_ids.begin(), fallback_class_ids.end());

        for (std::uint32_t class_id : fallback_class_ids) {
            const QVector<int> class_rows = rows_by_class_id.value(class_id);
            if (class_rows.isEmpty()) {
                continue;
            }

            // MFC shows base class presets directly under the category root.
            // Only sub-factories/classes should get an extra folder layer.
            if (class_id == category_id) {
                add_class_tree(category_item, class_rows);
                continue;
            }

            auto *class_item = new QTreeWidgetItem(category_item);
            class_item->setText(0, FactoryFolderNameForClassId(class_id));
            class_item->setText(1, QString::number(class_rows.size()));
            add_class_tree(class_item, class_rows);
            class_item->setExpanded(true);
        }

        category_item->setExpanded(true);
    }

    if (filter_text.isEmpty()) {
        _summary->setText(_baseSummaryText);
    } else {
        _summary->setText(QStringLiteral("%1 (showing %2 of %3)")
                              .arg(_baseSummaryText)
                              .arg(shown_count)
                              .arg(static_cast<qlonglong>(_rows.size())));
    }

    _tree->setUpdatesEnabled(true);
    if (_modButton != nullptr) {
        _modButton->setEnabled(isPresetRowItem(_tree->currentItem()));
    }
}

void PresetsPanel::emitPresetActivatedForItem(QTreeWidgetItem *item)
{
    if (item == nullptr) {
        emit presetActivated(0, QString(), 0, 0, false);
        return;
    }

    bool ok = false;
    const int row_index = item->data(0, kPresetRowRole).toInt(&ok);
    if (!ok || row_index < 0 || row_index >= static_cast<int>(_rows.size())) {
        emit presetActivated(0, QString(), 0, 0, false);
        return;
    }

    const RowData &row = _rows[static_cast<std::size_t>(row_index)];
    emit presetActivated(static_cast<quint32>(row.id),
                         row.name,
                         static_cast<quint32>(row.class_id),
                         static_cast<quint32>(row.parent_id),
                         row.temporary);
}

void PresetsPanel::emitPresetOpenForItem(QTreeWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    bool ok = false;
    const int row_index = item->data(0, kPresetRowRole).toInt(&ok);
    if (!ok || row_index < 0 || row_index >= static_cast<int>(_rows.size())) {
        return;
    }

    const RowData &row = _rows[static_cast<std::size_t>(row_index)];
    emit presetOpenRequested(static_cast<quint32>(row.id),
                             row.name,
                             static_cast<quint32>(row.class_id),
                             static_cast<quint32>(row.parent_id),
                             row.temporary);
}

bool PresetsPanel::isPresetRowItem(QTreeWidgetItem *item) const
{
    if (item == nullptr) {
        return false;
    }

    bool ok = false;
    const int row_index = item->data(0, kPresetRowRole).toInt(&ok);
    return ok && row_index >= 0 && row_index < static_cast<int>(_rows.size());
}

} // namespace leveledit_qt
