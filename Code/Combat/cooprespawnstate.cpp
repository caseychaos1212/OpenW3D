/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "cooprespawnstate.h"

#include "ccamera.h"
#include "clientcontrol.h"
#include "combat.h"
#include "gameobjmanager.h"
#include "scriptablegameobj.h"
#include "soldier.h"

static bool CoopRespawnWaiting = false;
static int CoopRespawnSpectateObjectId = 0;
static int CoopRespawnActiveSpectateObjectId = 0;

static SoldierGameObj *Find_Coop_Respawn_Spectate_Soldier(int object_id)
{
	if (object_id == 0) {
		return NULL;
	}

	ScriptableGameObj *obj = GameObjManager::Find_ScriptableGameObj(object_id);
	SoldierGameObj *soldier = obj != NULL ? obj->As_SoldierGameObj() : NULL;
	if (soldier == NULL ||
		 soldier->Is_Delete_Pending() ||
		 soldier->Is_Dead() ||
		 soldier->Is_Destroyed()) {
		return NULL;
	}

	return soldier;
}

static void Release_Coop_Respawn_Spectate_Soldier(void)
{
	if (CoopRespawnActiveSpectateObjectId == 0) {
		return;
	}

	SoldierGameObj *soldier = Find_Coop_Respawn_Spectate_Soldier(CoopRespawnActiveSpectateObjectId);
	if (soldier != NULL && soldier->Peek_Model() != NULL) {
		soldier->Peek_Model()->Set_Hidden(false);
	}

	if (COMBAT_STAR != NULL && COMBAT_STAR->Get_ID() == CoopRespawnActiveSpectateObjectId) {
		CombatManager::Set_The_Star(NULL, false);
	}

	CoopRespawnActiveSpectateObjectId = 0;
}

void cCoopRespawnState::Set_Waiting(bool waiting, int spectate_object_id)
{
	if (waiting &&
		 CoopRespawnActiveSpectateObjectId != 0 &&
		 CoopRespawnActiveSpectateObjectId != spectate_object_id) {
		Release_Coop_Respawn_Spectate_Soldier();
	}

	if (!waiting) {
		Release_Coop_Respawn_Spectate_Soldier();
		if (COMBAT_CAMERA != NULL && COMBAT_CAMERA->Is_Using_Host_Model()) {
			COMBAT_CAMERA->Set_Host_Model(NULL);
		}
		if (PClientControl != NULL && CombatManager::I_Am_Client()) {
			PClientControl->Set_Update_Flag(-1);
		}
	}

	CoopRespawnWaiting = waiting;
	CoopRespawnSpectateObjectId = waiting ? spectate_object_id : 0;

	if (waiting) {
		Update_Spectate_Camera();
	}
}

bool cCoopRespawnState::Is_Waiting(void)
{
	return CoopRespawnWaiting;
}

int cCoopRespawnState::Get_Spectate_Object_Id(void)
{
	return CoopRespawnSpectateObjectId;
}

void cCoopRespawnState::Update_Spectate_Camera(void)
{
	if (!CoopRespawnWaiting) {
		return;
	}

	SoldierGameObj *soldier = Find_Coop_Respawn_Spectate_Soldier(CoopRespawnSpectateObjectId);
	if (soldier == NULL) {
		if (CoopRespawnActiveSpectateObjectId == CoopRespawnSpectateObjectId) {
			Release_Coop_Respawn_Spectate_Soldier();
		}
		return;
	}

	if (CoopRespawnActiveSpectateObjectId != 0 &&
		 CoopRespawnActiveSpectateObjectId != CoopRespawnSpectateObjectId) {
		Release_Coop_Respawn_Spectate_Soldier();
	}

	CoopRespawnActiveSpectateObjectId = CoopRespawnSpectateObjectId;

	if (COMBAT_CAMERA != NULL && COMBAT_CAMERA->Is_Using_Host_Model()) {
		COMBAT_CAMERA->Set_Host_Model(NULL);
	}

	if (COMBAT_STAR != soldier) {
		CombatManager::Set_The_Star(soldier, false);
	} else {
		CombatManager::Set_Is_Star_Determining_Target(false);
	}

	if (COMBAT_CAMERA != NULL) {
		Vector3 soldier_position;
		soldier->Get_Position(&soldier_position);
		Vector3 target_offset = soldier->Get_Targeting_Pos() - soldier_position;
		if (target_offset.Length2() > 0.25F) {
			COMBAT_CAMERA->Force_Look(soldier->Get_Targeting_Pos());
		} else {
			COMBAT_CAMERA->Force_Heading(soldier->Get_Facing());
		}
	}

	if (PClientControl != NULL && CombatManager::I_Am_Client()) {
		PClientControl->Set_Update_Flag(-1);
	}
}

const WideStringClass &cCoopRespawnState::Get_Waiting_Text(void)
{
	static WideStringClass waiting_text(U_CHAR("Waiting for player to move to safe location"), true);
	return waiting_text;
}
