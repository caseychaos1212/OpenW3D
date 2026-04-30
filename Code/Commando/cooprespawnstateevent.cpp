/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "cooprespawnstateevent.h"

#include "apppackettypes.h"
#include "ccamera.h"
#include "cnetwork.h"
#include "combat.h"
#include "cooprespawnstate.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "networkobjectfactory.h"
#include "physicalgameobj.h"
#include "scriptablegameobj.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopRespawnStateEvent, NETCLASSID_COOPRESPAWNSTATEEVENT);

cCoopRespawnStateEvent::cCoopRespawnStateEvent(void) :
	Waiting(false),
	SpectateObjectId(0)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPRESPAWNSTATEEVENT);
}

void cCoopRespawnStateEvent::Init(int client_id, bool waiting, int spectate_object_id)
{
	WWASSERT(cNetwork::I_Am_Server());

	Waiting = waiting;
	SpectateObjectId = spectate_object_id;

	bool local_target = cNetwork::I_Am_Client() && client_id == cNetwork::Get_My_Id();
	if (local_target) {
		Act();
	}

	if (cNetwork::I_Am_Only_Server() || !local_target) {
		Set_Object_Dirty_Bit(client_id, BIT_CREATION, true);
	} else {
		Set_Delete_Pending();
	}
}

void cCoopRespawnStateEvent::Act(void)
{
	if (!IS_COOP_MISSION) {
		cCoopRespawnState::Set_Waiting(false, 0);
		return;
	}

	cCoopRespawnState::Set_Waiting(Waiting, SpectateObjectId);

	if (COMBAT_CAMERA == NULL) {
		return;
	}

	if (!Waiting) {
		COMBAT_CAMERA->Set_Host_Model(NULL);
		return;
	}

	ScriptableGameObj *obj = GameObjManager::Find_ScriptableGameObj(SpectateObjectId);
	PhysicalGameObj *physical = obj != NULL ? obj->As_PhysicalGameObj() : NULL;
	if (physical != NULL) {
		COMBAT_CAMERA->Set_Host_Model(physical->Peek_Model());
	}
}

void cCoopRespawnStateEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);
	packet.Add(Waiting);
	packet.Add(SpectateObjectId);

	Set_Delete_Pending();
}

void cCoopRespawnStateEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Only_Client());

	cNetEvent::Import_Creation(packet);
	packet.Get(Waiting);
	packet.Get(SpectateObjectId);

	Act();
	Set_Delete_Pending();
}
