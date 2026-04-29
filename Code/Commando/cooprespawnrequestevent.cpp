/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "cooprespawnrequestevent.h"

#include "apppackettypes.h"
#include "cnetwork.h"
#include "gametype.h"
#include "god.h"
#include "networkobjectfactory.h"
#include "networkobjectmgr.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopRespawnRequestEvent, NETCLASSID_COOPRESPAWNREQUESTEVENT);

cCoopRespawnRequestEvent::cCoopRespawnRequestEvent(void) :
	SenderId(0)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPRESPAWNREQUESTEVENT);
}

void cCoopRespawnRequestEvent::Init(void)
{
	WWASSERT(cNetwork::I_Am_Client());

	SenderId = cNetwork::Get_My_Id();
	Set_Network_ID(NetworkObjectMgrClass::Get_New_Client_ID());

	if (cNetwork::I_Am_Server()) {
		Act();
	} else {
		Set_Object_Dirty_Bit(0, BIT_CREATION, true);
	}
}

void cCoopRespawnRequestEvent::Act(void)
{
	WWASSERT(cNetwork::I_Am_Server());

	if (IS_COOP_MISSION && cGod::Can_Coop_Respawn_Player(SenderId)) {
		cGod::Coop_Respawn_Player(SenderId);
	}

	Set_Delete_Pending();
}

void cCoopRespawnRequestEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Client());

	cNetEvent::Export_Creation(packet);
	packet.Add(SenderId);

	Set_Delete_Pending();
}

void cCoopRespawnRequestEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Import_Creation(packet);
	packet.Get(SenderId);

	Act();
}
