/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "cooplobbyreadyevent.h"

#include "apppackettypes.h"
#include "cnetwork.h"
#include "cooplobbymgr.h"
#include "gametype.h"
#include "networkobjectfactory.h"
#include "networkobjectmgr.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopLobbyReadyEvent, NETCLASSID_COOPLOBBYREADYEVENT);

cCoopLobbyReadyEvent::cCoopLobbyReadyEvent(void) :
	SenderId(0),
	Ready(false)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPLOBBYREADYEVENT);
}

void cCoopLobbyReadyEvent::Init(bool ready)
{
	WWASSERT(cNetwork::I_Am_Client());

	SenderId = cNetwork::Get_My_Id();
	Ready = ready;
	Set_Network_ID(NetworkObjectMgrClass::Get_New_Client_ID());

	if (cNetwork::I_Am_Server()) {
		Act();
	} else {
		Set_Object_Dirty_Bit(0, BIT_CREATION, true);
	}
}

void cCoopLobbyReadyEvent::Act(void)
{
	WWASSERT(cNetwork::I_Am_Server());

	if (IS_COOP_MISSION && CoopLobbyMgrClass::Is_Active()) {
		CoopLobbyMgrClass::Set_Player_Ready(SenderId, Ready);
		CoopLobbyMgrClass::Broadcast_State();
	}

	Set_Delete_Pending();
}

void cCoopLobbyReadyEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Client());

	cNetEvent::Export_Creation(packet);
	packet.Add(SenderId);
	packet.Add(Ready);

	Set_Delete_Pending();
}

void cCoopLobbyReadyEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Import_Creation(packet);
	packet.Get(SenderId);
	packet.Get(Ready);

	Act();
}
