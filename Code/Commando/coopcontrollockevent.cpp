/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "coopcontrollockevent.h"

#include "apppackettypes.h"
#include "cnetwork.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "networkobjectfactory.h"
#include "player.h"
#include "playermanager.h"
#include "scriptcommands.h"
#include "smartgameobj.h"
#include "soldier.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopControlLockEvent, NETCLASSID_COOPCONTROLLOCKEVENT);

static bool ApplyingCoopControlLock = false;

static void Coop_Control_Enable_Callback(GameObject *obj, bool enable)
{
	if (ApplyingCoopControlLock || !IS_COOP_MISSION || !cNetwork::I_Am_Server() || obj == NULL) {
		return;
	}

	SmartGameObj *smart = obj->As_SmartGameObj();
	if (smart == NULL || smart->Get_Control_Owner() <= 0) {
		return;
	}

	ApplyingCoopControlLock = true;
	SList<cPlayer> *player_list = cPlayerManager::Get_Player_Object_List();
	for (SLNode<cPlayer> *node = player_list != NULL ? player_list->Head() : NULL; node; node = node->Next()) {
		cPlayer *player = node->Data();
		if (player == NULL || !player->Is_Active() || player->Get_Id() <= 0) {
			continue;
		}

		SoldierGameObj *soldier = GameObjManager::Find_Soldier_Of_Client_ID(player->Get_Id());
		if (soldier != NULL) {
			soldier->Control_Enable(enable);
		}
	}
	ApplyingCoopControlLock = false;

	cCoopControlLockEvent *event = new cCoopControlLockEvent;
	event->Init(enable);
}

class CoopControlLockRegistrationClass
{
public:
	CoopControlLockRegistrationClass(void)
	{
		ScriptCommands_Set_Control_Enable_Callback(Coop_Control_Enable_Callback);
	}
};

static CoopControlLockRegistrationClass CoopControlLockRegistration;

cCoopControlLockEvent::cCoopControlLockEvent(void) :
	Enabled(true)
{
	Set_App_Packet_Type(APPPACKETTYPE_COOPCONTROLLOCKEVENT);
}

void cCoopControlLockEvent::Init(bool enabled)
{
	WWASSERT(cNetwork::I_Am_Server());

	Enabled = enabled;
	Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopControlLockEvent::Act(void)
{
	if (!IS_COOP_MISSION) {
		return;
	}

	SoldierGameObj *soldier = GameObjManager::Find_Soldier_Of_Client_ID(cNetwork::Get_My_Id());
	if (soldier != NULL) {
		soldier->Control_Enable(Enabled);
	}
}

void cCoopControlLockEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);
	packet.Add(Enabled);

	Set_Delete_Pending();
}

void cCoopControlLockEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Only_Client());

	cNetEvent::Import_Creation(packet);
	packet.Get(Enabled);

	Act();
	Set_Delete_Pending();
}
