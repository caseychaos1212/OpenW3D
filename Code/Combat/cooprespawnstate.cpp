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

static bool CoopRespawnWaiting = false;
static int CoopRespawnSpectateObjectId = 0;

void cCoopRespawnState::Set_Waiting(bool waiting, int spectate_object_id)
{
	CoopRespawnWaiting = waiting;
	CoopRespawnSpectateObjectId = waiting ? spectate_object_id : 0;
}

bool cCoopRespawnState::Is_Waiting(void)
{
	return CoopRespawnWaiting;
}

int cCoopRespawnState::Get_Spectate_Object_Id(void)
{
	return CoopRespawnSpectateObjectId;
}

const WideStringClass &cCoopRespawnState::Get_Waiting_Text(void)
{
	static WideStringClass waiting_text(U_CHAR("Waiting for player to move to safe location"), true);
	return waiting_text;
}
