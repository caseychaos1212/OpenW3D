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
#include "gdcoopmission.h"
#include "gametype.h"
#include "networkobjectfactory.h"

DECLARE_NETWORKOBJECT_FACTORY(cCoopLevelTransitionEvent, NETCLASSID_COOPLEVELTRANSITIONEVENT);

//-----------------------------------------------------------------------------
cCoopLevelTransitionEvent::cCoopLevelTransitionEvent(void) :
	DifficultyLevel(0),
	EnableSprint(true),
	DisableHealthPickups(false),
	DisableArmorPickups(false),
	DisableAmmoPickups(false),
	EnemyHealthMultiplier(1.0f),
	EnemyDamageMultiplier(1.0f),
	DeathScorePenalty(100)
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

	cGameDataCoopMission *coop_game = The_Game() != NULL ? The_Game()->As_Coop_Mission() : NULL;
	if (coop_game != NULL) {
		DifficultyLevel = coop_game->Get_Difficulty_Level();
		EnableSprint = coop_game->Is_Sprint_Enabled();
		DisableHealthPickups = coop_game->Are_Health_Pickups_Disabled();
		DisableArmorPickups = coop_game->Are_Armor_Pickups_Disabled();
		DisableAmmoPickups = coop_game->Are_Ammo_Pickups_Disabled();
		EnemyHealthMultiplier = coop_game->Get_Enemy_Health_Multiplier();
		EnemyDamageMultiplier = coop_game->Get_Enemy_Damage_Multiplier();
		DeathScorePenalty = coop_game->Get_Death_Score_Penalty();
	}

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
cCoopLevelTransitionEvent::Apply_Settings_To_Game_Data(void) const
{
	cGameDataCoopMission *coop_game = The_Game() != NULL ? The_Game()->As_Coop_Mission() : NULL;
	if (coop_game == NULL) {
		return;
	}

	coop_game->Set_Difficulty_Level(DifficultyLevel);
	coop_game->Set_Sprint_Enabled(EnableSprint);
	coop_game->Set_Health_Pickups_Disabled(DisableHealthPickups);
	coop_game->Set_Armor_Pickups_Disabled(DisableArmorPickups);
	coop_game->Set_Ammo_Pickups_Disabled(DisableAmmoPickups);
	coop_game->Set_Enemy_Health_Multiplier(EnemyHealthMultiplier);
	coop_game->Set_Enemy_Damage_Multiplier(EnemyDamageMultiplier);
	coop_game->Set_Death_Score_Penalty(DeathScorePenalty);
	coop_game->Apply_Global_Settings();
}

//-----------------------------------------------------------------------------
void
cCoopLevelTransitionEvent::Export_Creation(BitStreamClass & packet)
{
	WWASSERT(cNetwork::I_Am_Server());

	cNetEvent::Export_Creation(packet);

	packet.Add_Terminated_String(MapName, true);
	packet.Add(DifficultyLevel);
	packet.Add(EnableSprint);
	packet.Add(DisableHealthPickups);
	packet.Add(DisableArmorPickups);
	packet.Add(DisableAmmoPickups);
	packet.Add(EnemyHealthMultiplier);
	packet.Add(EnemyDamageMultiplier);
	packet.Add(DeathScorePenalty);

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
	EnableSprint = packet.Get(EnableSprint);
	DisableHealthPickups = packet.Get(DisableHealthPickups);
	DisableArmorPickups = packet.Get(DisableArmorPickups);
	DisableAmmoPickups = packet.Get(DisableAmmoPickups);
	EnemyHealthMultiplier = packet.Get(EnemyHealthMultiplier);
	EnemyDamageMultiplier = packet.Get(EnemyDamageMultiplier);
	DeathScorePenalty = packet.Get(DeathScorePenalty);

	Apply_Settings_To_Game_Data();
	Act();
	Set_Delete_Pending();
}
