/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "coopcameraevent.h"

#include "apppackettypes.h"
#include "ccamera.h"
#include "cnetwork.h"
#include "combat.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "networkobjectfactory.h"
#include "physicalgameobj.h"
#include "scriptablegameobj.h"
#include "scriptcommands.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopCameraEvent, NETCLASSID_COOPCAMERAEVENT);

static bool ApplyingCoopCameraEvent = false;
static int CurrentCoopCameraHostId = -1;
static bool HasCurrentCoopForceLook = false;
static Vector3 CurrentCoopForceLook(0.0F, 0.0F, 0.0F);

static void Coop_Camera_Host_Callback(GameObject *obj)
{
	if (ApplyingCoopCameraEvent || !IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return;
	}

	CurrentCoopCameraHostId = (obj != NULL) ? obj->Get_ID() : 0;
	if (obj == NULL) {
		HasCurrentCoopForceLook = false;
	}

	cCoopCameraEvent *event = new cCoopCameraEvent;
	event->Init_Camera_Host(obj);
}

static void Coop_Force_Camera_Look_Callback(const Vector3 &target)
{
	if (ApplyingCoopCameraEvent || !IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return;
	}

	HasCurrentCoopForceLook = true;
	CurrentCoopForceLook = target;

	cCoopCameraEvent *event = new cCoopCameraEvent;
	event->Init_Force_Look(target);
}

class CoopCameraEventRegistrationClass
{
public:
	CoopCameraEventRegistrationClass(void)
	{
		ScriptCommands_Set_Camera_Host_Callback(Coop_Camera_Host_Callback);
		ScriptCommands_Set_Force_Camera_Look_Callback(Coop_Force_Camera_Look_Callback);
	}
};

static CoopCameraEventRegistrationClass CoopCameraEventRegistration;

cCoopCameraEvent::cCoopCameraEvent(void) :
	Operation(OP_CAMERA_HOST),
	HostObjectId(0),
	Target(0.0F, 0.0F, 0.0F)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPCAMERAEVENT);
}

void cCoopCameraEvent::Init_Camera_Host(GameObject *obj)
{
	WWASSERT(cNetwork::I_Am_Server());

	Operation = OP_CAMERA_HOST;
	HostObjectId = (obj != NULL) ? obj->Get_ID() : 0;
	Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopCameraEvent::Sync_Current_Camera_State(void)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return;
	}

	if (CurrentCoopCameraHostId >= 0) {
		cCoopCameraEvent *event = new cCoopCameraEvent;
		event->Operation = OP_CAMERA_HOST;
		event->HostObjectId = CurrentCoopCameraHostId;
		event->Set_Object_Dirty_Bit(BIT_CREATION, true);
	}

	if (HasCurrentCoopForceLook) {
		cCoopCameraEvent *event = new cCoopCameraEvent;
		event->Init_Force_Look(CurrentCoopForceLook);
	}
}

void cCoopCameraEvent::Init_Force_Look(const Vector3 &target)
{
	WWASSERT(cNetwork::I_Am_Server());

	Operation = OP_FORCE_LOOK;
	Target = target;
	Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopCameraEvent::Act(void)
{
	if (!IS_COOP_MISSION || COMBAT_CAMERA == NULL) {
		return;
	}

	ApplyingCoopCameraEvent = true;

	if (Operation == OP_CAMERA_HOST) {
		if (HostObjectId == 0) {
			COMBAT_CAMERA->Set_Host_Model(NULL);
		} else {
			ScriptableGameObj *obj = GameObjManager::Find_ScriptableGameObj(HostObjectId);
			PhysicalGameObj *physical = (obj != NULL) ? obj->As_PhysicalGameObj() : NULL;
			if (physical != NULL) {
				COMBAT_CAMERA->Set_Host_Model(physical->Peek_Model());
			}
		}
	} else if (Operation == OP_FORCE_LOOK) {
		COMBAT_CAMERA->Force_Look(Target);
	}

	ApplyingCoopCameraEvent = false;
}

void cCoopCameraEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);
	packet.Add(Operation);
	packet.Add(HostObjectId);
	packet.Add(Target.X);
	packet.Add(Target.Y);
	packet.Add(Target.Z);

	Set_Delete_Pending();
}

void cCoopCameraEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Only_Client());

	cNetEvent::Import_Creation(packet);
	packet.Get(Operation);
	packet.Get(HostObjectId);
	packet.Get(Target.X);
	packet.Get(Target.Y);
	packet.Get(Target.Z);

	Act();
	Set_Delete_Pending();
}
