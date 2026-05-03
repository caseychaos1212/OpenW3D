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
#include "cooplobbymgr.h"
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
	DeathScorePenalty(100),
	AISightMultiplier(1.25f),
	AIHearingMultiplier(1.5f),
	AIAggressivenessBonus(0.15f),
	AITakeCoverBonus(0.15f),
	AIShareInfoRadius(15.0f),
	AIWeaponErrorMultiplier(0.75f),
	AISpecialDamageStateLockChance(0.35f),
	AIEnableAttackWander(true),
	AIEnableDamageRetarget(true),
	AIEnableUnitCombatTypes(true)
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
		AISightMultiplier = coop_game->Get_AI_Sight_Multiplier();
		AIHearingMultiplier = coop_game->Get_AI_Hearing_Multiplier();
		AIAggressivenessBonus = coop_game->Get_AI_Aggressiveness_Bonus();
		AITakeCoverBonus = coop_game->Get_AI_Take_Cover_Bonus();
		AIShareInfoRadius = coop_game->Get_AI_Share_Info_Radius();
		AIWeaponErrorMultiplier = coop_game->Get_AI_Weapon_Error_Multiplier();
		AISpecialDamageStateLockChance = coop_game->Get_AI_Special_Damage_State_Lock_Chance();
		AIEnableAttackWander = coop_game->Is_AI_Attack_Wander_Enabled();
		AIEnableDamageRetarget = coop_game->Is_AI_Damage_Retarget_Enabled();
		AIEnableUnitCombatTypes = coop_game->Are_AI_Unit_Combat_Types_Enabled();
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

	CoopLobbyMgrClass::Close();
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
	coop_game->Set_AI_Sight_Multiplier(AISightMultiplier);
	coop_game->Set_AI_Hearing_Multiplier(AIHearingMultiplier);
	coop_game->Set_AI_Aggressiveness_Bonus(AIAggressivenessBonus);
	coop_game->Set_AI_Take_Cover_Bonus(AITakeCoverBonus);
	coop_game->Set_AI_Share_Info_Radius(AIShareInfoRadius);
	coop_game->Set_AI_Weapon_Error_Multiplier(AIWeaponErrorMultiplier);
	coop_game->Set_AI_Special_Damage_State_Lock_Chance(AISpecialDamageStateLockChance);
	coop_game->Set_AI_Attack_Wander_Enabled(AIEnableAttackWander);
	coop_game->Set_AI_Damage_Retarget_Enabled(AIEnableDamageRetarget);
	coop_game->Set_AI_Unit_Combat_Types_Enabled(AIEnableUnitCombatTypes);
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
	packet.Add(AISightMultiplier);
	packet.Add(AIHearingMultiplier);
	packet.Add(AIAggressivenessBonus);
	packet.Add(AITakeCoverBonus);
	packet.Add(AIShareInfoRadius);
	packet.Add(AIWeaponErrorMultiplier);
	packet.Add(AISpecialDamageStateLockChance);
	packet.Add(AIEnableAttackWander);
	packet.Add(AIEnableDamageRetarget);
	packet.Add(AIEnableUnitCombatTypes);

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
	AISightMultiplier = packet.Get(AISightMultiplier);
	AIHearingMultiplier = packet.Get(AIHearingMultiplier);
	AIAggressivenessBonus = packet.Get(AIAggressivenessBonus);
	AITakeCoverBonus = packet.Get(AITakeCoverBonus);
	AIShareInfoRadius = packet.Get(AIShareInfoRadius);
	AIWeaponErrorMultiplier = packet.Get(AIWeaponErrorMultiplier);
	AISpecialDamageStateLockChance = packet.Get(AISpecialDamageStateLockChance);
	AIEnableAttackWander = packet.Get(AIEnableAttackWander);
	AIEnableDamageRetarget = packet.Get(AIEnableDamageRetarget);
	AIEnableUnitCombatTypes = packet.Get(AIEnableUnitCombatTypes);

	Apply_Settings_To_Game_Data();
	Act();
	Set_Delete_Pending();
}
