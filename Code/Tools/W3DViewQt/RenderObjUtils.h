#pragma once

class RenderObjClass;
class HLodClass;

void UpdateLodPrototype(HLodClass &hlod);
void UpdateAggregatePrototype(RenderObjClass &render_obj);
bool RenameAggregatePrototype(const char *old_name, const char *new_name);
