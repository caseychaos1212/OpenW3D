/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "coopleveltransitionevent.h"

#include <cstring>

#include "apppackettypes.h"
#include "cnetwork.h"
#include "gameinitmgr.h"
#include "gametype.h"
#include "networkobjectfactory.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopLevelTransitionEvent, NETCLASSID_COOPLEVELTRANSITIONEVENT);

//-----------------------------------------------------------------------------
cCoopLevelTransitionEvent::cCoopLevelTransitionEvent(void) :
	DifficultyLevel(0)
{
	MapName[0] = 0;
	Set_App_Packet_Type(APPPACKETTYPE_COOPLEVELTRANSITIONEVENT);
}

//-----------------------------------------------------------------------------
void
cCoopLevelTransitionEvent::Init(const char *map_name, int difficulty_level)
{
	WWASSERT(cNetwork::I_Am_Server());
	WWASSERT(map_name != NULL);

	::strncpy(MapName, map_name, sizeof(MapName) - 1);
	MapName[sizeof(MapName) - 1] = 0;
	DifficultyLevel = difficulty_level;

	Set_Object_Dirty_Bit(BIT_CREATION, true);
}

//-----------------------------------------------------------------------------
void
cCoopLevelTransitionEvent::Act(void)
{
	if (!IS_COOP_MISSION || MapName[0] == 0) {
		return;
	}

	GameInitMgrClass::Queue_Coop_Level_Transition(MapName, DifficultyLevel);
}

//-----------------------------------------------------------------------------
void
cCoopLevelTransitionEvent::Export_Creation(BitStreamClass & packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);

	packet.Add_Terminated_String(MapName, true);
	packet.Add(DifficultyLevel);

	Set_Delete_Pending();
}

//-----------------------------------------------------------------------------
void
cCoopLevelTransitionEvent::Import_Creation(BitStreamClass & packet)
{
	cNetEvent::Import_Creation(packet);

	WWASSERT(cNetwork::I_Am_Only_Client());

	packet.Get_Terminated_String(MapName, sizeof(MapName), true);
	DifficultyLevel = packet.Get(DifficultyLevel);

	Act();
	Set_Delete_Pending();
}
