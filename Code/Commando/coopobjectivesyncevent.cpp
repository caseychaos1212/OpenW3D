/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "coopobjectivesyncevent.h"

#include "apppackettypes.h"
#include "cnetwork.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "networkobjectfactory.h"
#include "physicalgameobj.h"

enum {
	COOP_OBJECTIVE_SYNC_SNAPSHOT = 100
};

DECLARE_NETWORKOBJECT_FACTORY(cCoopObjectiveSyncEvent, NETCLASSID_COOPOBJECTIVESYNCEVENT);

static Objective *Find_Objective_By_ID(int id)
{
	for (int index = 0; index < ObjectiveManager::Get_Objective_Count(); index++) {
		Objective *objective = ObjectiveManager::Get_Objective(index);
		if (objective != NULL && objective->ID == id) {
			return objective;
		}
	}

	return NULL;
}

static void Coop_Objective_Sync_Callback(int operation, const Objective *objective, int objective_id)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return;
	}

	cCoopObjectiveSyncEvent *event = new cCoopObjectiveSyncEvent;
	if (operation == ObjectiveManager::COOP_SYNC_REMOVE) {
		event->Init_Remove(objective_id);
	} else if (objective != NULL) {
		event->Init_Update(operation, objective);
	} else {
		delete event;
	}
}

class CoopObjectiveSyncRegistrationClass
{
public:
	CoopObjectiveSyncRegistrationClass(void)
	{
		ObjectiveManager::Set_Coop_Sync_Callback(Coop_Objective_Sync_Callback);
	}
};

static CoopObjectiveSyncRegistrationClass CoopObjectiveSyncRegistration;

cCoopObjectiveSyncEvent::cCoopObjectiveSyncEvent(void) :
	Operation(ObjectiveManager::COOP_SYNC_ADD),
	ObjectiveID(0)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPOBJECTIVESYNCEVENT);
}

void cCoopObjectiveSyncEvent::Init_Snapshot(int client_id)
{
	WWASSERT(cNetwork::I_Am_Server());

	Operation = COOP_OBJECTIVE_SYNC_SNAPSHOT;
	ObjectiveID = 0;

	if (client_id > 0) {
		Set_Object_Dirty_Bit(client_id, BIT_CREATION, true);
	} else {
		Set_Object_Dirty_Bit(BIT_CREATION, true);
	}
}

void cCoopObjectiveSyncEvent::Init_Update(int operation, const Objective *objective)
{
	WWASSERT(cNetwork::I_Am_Server());
	WWASSERT(objective != NULL);

	Operation = operation;
	ObjectiveID = objective->ID;
	Copy_Objective(State, objective);

	Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopObjectiveSyncEvent::Init_Remove(int objective_id)
{
	WWASSERT(cNetwork::I_Am_Server());

	Operation = ObjectiveManager::COOP_SYNC_REMOVE;
	ObjectiveID = objective_id;

	Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopObjectiveSyncEvent::Send_Snapshot(int client_id)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return;
	}

	cCoopObjectiveSyncEvent *event = new cCoopObjectiveSyncEvent;
	event->Init_Snapshot(client_id);
}

void cCoopObjectiveSyncEvent::Copy_Objective(ObjectiveState &state, const Objective *objective)
{
	WWASSERT(objective != NULL);

	state.ID = objective->ID;
	state.Type = objective->Type;
	state.Status = objective->Status;
	state.LongDescriptionID = objective->LongDescriptionID;
	state.ShortDescriptionID = objective->ShortDescriptionID;
	state.DescriptionSoundFilename = objective->DescriptionSoundFilename;
	state.HUDPogTextureName = objective->HUDPogTextureName;
	state.HUDMessageStringID = objective->HUDMessageStringID;
	state.HUDPriority = objective->HUDPriority;
	state.DrawBlip = objective->DrawBlip;
	state.Position = objective->Position;
	state.BlipIntensity = objective->BlipIntensity;

	state.ObjectID = 0;
	PhysicalGameObj *object = (PhysicalGameObj *)objective->Object.Get_Ptr();
	if (object != NULL) {
		state.ObjectID = object->Get_ID();
	}
}

void cCoopObjectiveSyncEvent::Export_Objective(BitStreamClass &packet, const ObjectiveState &state)
{
	packet.Add(state.ID);
	packet.Add(state.Type);
	packet.Add(state.Status);
	packet.Add(state.LongDescriptionID);
	packet.Add(state.ShortDescriptionID);
	packet.Add_Terminated_String(state.DescriptionSoundFilename.Peek_Buffer(), true);
	packet.Add_Terminated_String(state.HUDPogTextureName.Peek_Buffer(), true);
	packet.Add(state.HUDMessageStringID);
	packet.Add(state.HUDPriority);
	packet.Add(state.DrawBlip);
	packet.Add(state.Position.X);
	packet.Add(state.Position.Y);
	packet.Add(state.Position.Z);
	packet.Add(state.BlipIntensity);
	packet.Add(state.ObjectID);
}

void cCoopObjectiveSyncEvent::Import_Objective(BitStreamClass &packet, ObjectiveState &state)
{
	packet.Get(state.ID);
	packet.Get(state.Type);
	packet.Get(state.Status);
	packet.Get(state.LongDescriptionID);
	packet.Get(state.ShortDescriptionID);
	packet.Get_Terminated_String(state.DescriptionSoundFilename.Get_Buffer(256), 256, true);
	packet.Get_Terminated_String(state.HUDPogTextureName.Get_Buffer(256), 256, true);
	packet.Get(state.HUDMessageStringID);
	packet.Get(state.HUDPriority);
	packet.Get(state.DrawBlip);
	packet.Get(state.Position.X);
	packet.Get(state.Position.Y);
	packet.Get(state.Position.Z);
	packet.Get(state.BlipIntensity);
	packet.Get(state.ObjectID);
}

void cCoopObjectiveSyncEvent::Apply_Objective(const ObjectiveState &state)
{
	Objective *objective = Find_Objective_By_ID(state.ID);
	if (objective == NULL) {
		ObjectiveManager::Add_Objective(
			state.ID,
			state.Type,
			state.Status,
			state.ShortDescriptionID,
			state.LongDescriptionID,
			state.DescriptionSoundFilename.Peek_Buffer());
		objective = Find_Objective_By_ID(state.ID);
	} else {
		if (objective->Type != state.Type) {
			ObjectiveManager::Change_Objective_Type(state.ID, state.Type);
		}
		if (objective->Status != state.Status) {
			ObjectiveManager::Set_Objective_Status(state.ID, state.Status);
		}
	}

	PhysicalGameObj *object = NULL;
	if (state.ObjectID != 0) {
		object = GameObjManager::Find_PhysicalGameObj(state.ObjectID);
	}

	if (object != NULL) {
		ObjectiveManager::Set_Objective_Radar_Blip(state.ID, object);
	} else if (state.DrawBlip) {
		ObjectiveManager::Set_Objective_Radar_Blip(state.ID, state.Position);
	}

	if (state.HUDPriority > 0.0f ||
		 !state.HUDPogTextureName.Is_Empty() ||
		 state.HUDMessageStringID != 0) {
		ObjectiveManager::Set_Objective_HUD_Info(
			state.ID,
			state.HUDPriority,
			state.HUDPogTextureName.Peek_Buffer(),
			state.HUDMessageStringID,
			state.Position);
	}
}

void cCoopObjectiveSyncEvent::Act(void)
{
}

void cCoopObjectiveSyncEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);

	packet.Add(Operation);
	if (Operation == COOP_OBJECTIVE_SYNC_SNAPSHOT) {
		int count = ObjectiveManager::Get_Objective_Count();
		packet.Add(count);
		for (int index = 0; index < count; index++) {
			ObjectiveState state;
			Copy_Objective(state, ObjectiveManager::Get_Objective(index));
			Export_Objective(packet, state);
		}
	} else if (Operation == ObjectiveManager::COOP_SYNC_REMOVE) {
		packet.Add(ObjectiveID);
	} else {
		Export_Objective(packet, State);
	}

	Set_Delete_Pending();
}

void cCoopObjectiveSyncEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Only_Client());

	cNetEvent::Import_Creation(packet);

	packet.Get(Operation);
	if (Operation == COOP_OBJECTIVE_SYNC_SNAPSHOT) {
		ObjectiveManager::Reset();

		int count = 0;
		packet.Get(count);
		for (int index = 0; index < count; index++) {
			ObjectiveState state;
			Import_Objective(packet, state);
			Apply_Objective(state);
		}
	} else if (Operation == ObjectiveManager::COOP_SYNC_REMOVE) {
		packet.Get(ObjectiveID);
		ObjectiveManager::Remove_Objective(ObjectiveID);
	} else {
		Import_Objective(packet, State);
		Apply_Objective(State);
	}

	Set_Delete_Pending();
}
