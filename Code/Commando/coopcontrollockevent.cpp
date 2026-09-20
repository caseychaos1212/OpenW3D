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
#include "gamedata.h"
#include <cstring>
#include "networkobjectfactory.h"
#include "player.h"
#include "playermanager.h"
#include "scriptcommands.h"
#include "smartgameobj.h"
#include "soldier.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopControlLockEvent, NETCLASSID_COOPCONTROLLOCKEVENT);

static bool ApplyingCoopControlLock = false;
static bool CurrentControlEnabled = true;
static bool HasControlState = false;
static int LastControlBodyId = 0;
static unsigned int ControlSequence = 0;
static unsigned int LastControlSequence = 0;

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
    Sequence(0), Enabled(true)
{
    Set_App_Packet_Type(APPPACKETTYPE_COOPCONTROLLOCKEVENT);
    if (The_Game() != nullptr) {
        MapName = The_Game()->Get_Map_Name();
    }
}

void cCoopControlLockEvent::Init(bool enabled)
{
	WWASSERT(cNetwork::I_Am_Server());

    Enabled = enabled;
    Sequence = ++ControlSequence;
    CurrentControlEnabled = enabled;
    HasControlState = true;
    Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopControlLockEvent::Act(void)
{
    if (!IS_COOP_MISSION || The_Game() == nullptr ||
        std::strcmp(MapName.Peek_Buffer(), The_Game()->Get_Map_Name().Peek_Buffer()) != 0 ||
        Sequence <= LastControlSequence) {
        return;
    }
    LastControlSequence = Sequence;
    CurrentControlEnabled = Enabled;
    HasControlState = true;
    LastControlBodyId = 0;
    Update();
}

void cCoopControlLockEvent::Export_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);
    packet.Add_Terminated_String(MapName.Peek_Buffer(), true);
    packet.Add(Sequence);
    packet.Add(Enabled);

	Set_Delete_Pending();
}

void cCoopControlLockEvent::Import_Creation(BitStreamClass &packet)
{
	WWASSERT(cNetwork::I_Am_Only_Client());

	cNetEvent::Import_Creation(packet);
    char map_name[256];
    packet.Get_Terminated_String(map_name, sizeof(map_name), true);
    MapName = map_name;
    packet.Get(Sequence);
    packet.Get(Enabled);

	Act();
	Set_Delete_Pending();
}

void cCoopControlLockEvent::Reset(bool new_session)
{
    if (new_session) {
        ControlSequence = 0;
        LastControlSequence = 0;
    }
    CurrentControlEnabled = true;
    HasControlState = false;
    LastControlBodyId = 0;
}

void cCoopControlLockEvent::Apply_Current_Control_State(SoldierGameObj *soldier)
{
    if (IS_COOP_MISSION && HasControlState && soldier != nullptr) {
        soldier->Control_Enable(CurrentControlEnabled);
    }
}

void cCoopControlLockEvent::Update()
{
    if (!IS_COOP_MISSION || !cNetwork::I_Am_Only_Client() || !HasControlState) {
        return;
    }
    SoldierGameObj *soldier = GameObjManager::Find_Soldier_Of_Client_ID(cNetwork::Get_My_Id());
    if (soldier != nullptr && soldier->Get_ID() != LastControlBodyId) {
        Apply_Current_Control_State(soldier);
        LastControlBodyId = soldier->Get_ID();
    }
}

void cCoopControlLockEvent::Sync_Current_Control_State(int client_id)
{
    if (!IS_COOP_MISSION || !cNetwork::I_Am_Server() || client_id <= 0 ||
        (cNetwork::I_Am_Client() && client_id == cNetwork::Get_My_Id())) {
        return;
    }
    cCoopControlLockEvent *event = new cCoopControlLockEvent;
    event->Enabled = CurrentControlEnabled;
    event->Sequence = ++ControlSequence;
    event->Set_Object_Dirty_Bit(client_id, BIT_CREATION, true);
}
