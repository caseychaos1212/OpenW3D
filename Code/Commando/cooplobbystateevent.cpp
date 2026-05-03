/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "cooplobbystateevent.h"

#include "apppackettypes.h"
#include "cnetwork.h"
#include "cooplobbymgr.h"
#include "networkobjectfactory.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopLobbyStateEvent, NETCLASSID_COOPLOBBYSTATEEVENT);

cCoopLobbyStateEvent::cCoopLobbyStateEvent(void)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPLOBBYSTATEEVENT);
}

void cCoopLobbyStateEvent::Init(int client_id)
{
	WWASSERT(cNetwork::I_Am_Server());

	if (client_id > 0) {
		Set_Object_Dirty_Bit(client_id, BIT_CREATION, true);
	} else {
		Set_Object_Dirty_Bit(BIT_CREATION, true);
	}
}

void cCoopLobbyStateEvent::Act(void)
{
	Set_Delete_Pending();
}

void cCoopLobbyStateEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);
	CoopLobbyMgrClass::Export_State(packet);

	Set_Delete_Pending();
}

void cCoopLobbyStateEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Only_Client());

	cNetEvent::Import_Creation(packet);
	CoopLobbyMgrClass::Import_State(packet);

	Set_Delete_Pending();
}
