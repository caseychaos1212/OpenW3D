#include "RenderObjUtils.h"

#include "agg_def.h"
#include "assetmgr.h"
#include "hlod.h"

#include <QString>

void UpdateLodPrototype(HLodClass &hlod)
{
    auto *definition = new HLodDefClass(hlod);
    auto *prototype = new HLodPrototypeClass(definition);

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        delete prototype;
        return;
    }

    asset_manager->Remove_Prototype(definition->Get_Name());
    asset_manager->Add_Prototype(prototype);
}

void UpdateAggregatePrototype(RenderObjClass &render_obj)
{
    auto *definition = new AggregateDefClass(render_obj);
    auto *prototype = new AggregatePrototypeClass(definition);

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        delete prototype;
        return;
    }

    asset_manager->Remove_Prototype(definition->Get_Name());
    asset_manager->Add_Prototype(prototype);
}

bool RenameAggregatePrototype(const char *old_name, const char *new_name)
{
    if (!old_name || !new_name) {
        return false;
    }

    const QString old_text = QString::fromLatin1(old_name);
    const QString new_text = QString::fromLatin1(new_name);
    if (old_text.compare(new_text, Qt::CaseInsensitive) == 0) {
        return false;
    }

    auto *asset_manager = WW3DAssetManager::Get_Instance();
    if (!asset_manager) {
        return false;
    }

    auto *proto = static_cast<AggregatePrototypeClass *>(asset_manager->Find_Prototype(old_name));
    if (!proto) {
        return false;
    }

    AggregateDefClass *definition = proto->Get_Definition();
    if (!definition) {
        return false;
    }

    AggregateDefClass *new_definition = definition->Clone();
    asset_manager->Remove_Prototype(old_name);

    new_definition->Set_Name(new_name);
    auto *new_proto = new AggregatePrototypeClass(new_definition);
    asset_manager->Add_Prototype(new_proto);
    return true;
}
