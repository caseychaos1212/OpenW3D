/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "coopcharacterselectevent.h"

#include <cstring>

#include "apppackettypes.h"
#include "cnetwork.h"
#include "gametype.h"
#include "god.h"
#include "networkobjectfactory.h"
#include "networkobjectmgr.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopCharacterSelectEvent, NETCLASSID_COOPCHARACTERSELECTEVENT);

cCoopCharacterSelectEvent::cCoopCharacterSelectEvent(void) :
	SenderId(0)
{
	PresetName[0] = 0;
	Set_App_Packet_Type(APPPACKETTYPE_COOPCHARACTERSELECTEVENT);
}

void cCoopCharacterSelectEvent::Init(const char *preset_name)
{
	WWASSERT(cNetwork::I_Am_Client());
	WWASSERT(preset_name != NULL);

	SenderId = cNetwork::Get_My_Id();
	::strncpy(PresetName, preset_name != NULL ? preset_name : "", sizeof(PresetName) - 1);
	PresetName[sizeof(PresetName) - 1] = 0;
	Set_Network_ID(NetworkObjectMgrClass::Get_New_Client_ID());

	if (cNetwork::I_Am_Server()) {
		Act();
	} else {
		Set_Object_Dirty_Bit(0, BIT_CREATION, true);
	}
}

void cCoopCharacterSelectEvent::Act(void)
{
	WWASSERT(cNetwork::I_Am_Server());

	if (IS_COOP_MISSION && PresetName[0] != 0) {
		cGod::Set_Coop_Character_Preset(SenderId, PresetName);
	}

	Set_Delete_Pending();
}

void cCoopCharacterSelectEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Client());

	cNetEvent::Export_Creation(packet);
	packet.Add(SenderId);
	packet.Add_Terminated_String(PresetName, true);

	Set_Delete_Pending();
}

void cCoopCharacterSelectEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Import_Creation(packet);
	packet.Get(SenderId);
	packet.Get_Terminated_String(PresetName, sizeof(PresetName), true);

	Act();
}
