#pragma once

#include <QStringList>

class RenderObjClass;
class HLodClass;

void UpdateLodPrototype(HLodClass &hlod);
void UpdateAggregatePrototype(RenderObjClass &render_obj);
bool RenameAggregatePrototype(const char *old_name, const char *new_name);
void CollectEmitterNames(RenderObjClass &render_obj, QStringList &names);
int CountParticles(RenderObjClass *render_obj);
