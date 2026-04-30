/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef GDCOOPMISSION_H
#define GDCOOPMISSION_H

#include "gamedata.h"

class cGameDataCoopMission : public cGameData
{
public:
	cGameDataCoopMission(void);
	~cGameDataCoopMission(void);

	bool	Is_Coop_Mission(void) const override				{return true;}
	cGameDataCoopMission * As_Coop_Mission(void) override	{return this;}

	static const unichar_t* Get_Static_Game_Name(void);

	const unichar_t*	Get_Game_Name(void) const override	{return Get_Static_Game_Name();}
	GameTypeEnum		Get_Game_Type(void) const override	{return GAME_TYPE_COOP_MISSION;}
	bool					Is_Limited(void) const override		{return true;}
	int					Choose_Player_Type(cPlayer* player, int team_choice, bool is_grunt) override;
	int					Get_Min_Players(void) const override	{return 1;}

	bool					Is_Editable_Friendly_Fire(void) const override {return true;}

	void					Load_From_Server_Config(void) override;
	void					Save_To_Server_Config(void) override;
	void					Export_Tier_2_Data(cPacket & packet) override;
	void					Import_Tier_2_Data(cPacket & packet) override;
	void					Apply_Global_Settings(void) const;

	const StringClass &	Get_Player2_Preset(void) const {return Player2Preset;}
	void					Set_Player2_Preset(const StringClass & preset) {Player2Preset = preset;}
	int					Get_Difficulty_Level(void) const {return DifficultyLevel;}
	void					Set_Difficulty_Level(int level);
	bool					Is_Sprint_Enabled(void) const {return EnableSprint;}
	void					Set_Sprint_Enabled(bool enabled);
	bool					Are_Health_Pickups_Disabled(void) const {return DisableHealthPickups;}
	void					Set_Health_Pickups_Disabled(bool disabled);
	bool					Are_Armor_Pickups_Disabled(void) const {return DisableArmorPickups;}
	void					Set_Armor_Pickups_Disabled(bool disabled);
	bool					Are_Ammo_Pickups_Disabled(void) const {return DisableAmmoPickups;}
	void					Set_Ammo_Pickups_Disabled(bool disabled);
	float					Get_Enemy_Health_Multiplier(void) const {return EnemyHealthMultiplier;}
	void					Set_Enemy_Health_Multiplier(float multiplier);
	float					Get_Enemy_Damage_Multiplier(void) const {return EnemyDamageMultiplier;}
	void					Set_Enemy_Damage_Multiplier(float multiplier);
	int					Get_Death_Score_Penalty(void) const {return DeathScorePenalty;}
	void					Set_Death_Score_Penalty(int penalty);
	float					Get_AI_Sight_Multiplier(void) const {return AISightMultiplier;}
	void					Set_AI_Sight_Multiplier(float multiplier);
	float					Get_AI_Hearing_Multiplier(void) const {return AIHearingMultiplier;}
	void					Set_AI_Hearing_Multiplier(float multiplier);
	float					Get_AI_Aggressiveness_Bonus(void) const {return AIAggressivenessBonus;}
	void					Set_AI_Aggressiveness_Bonus(float bonus);
	float					Get_AI_Take_Cover_Bonus(void) const {return AITakeCoverBonus;}
	void					Set_AI_Take_Cover_Bonus(float bonus);
	float					Get_AI_Share_Info_Radius(void) const {return AIShareInfoRadius;}
	void					Set_AI_Share_Info_Radius(float radius);
	float					Get_AI_Weapon_Error_Multiplier(void) const {return AIWeaponErrorMultiplier;}
	void					Set_AI_Weapon_Error_Multiplier(float multiplier);
	float					Get_AI_Special_Damage_State_Lock_Chance(void) const {return AISpecialDamageStateLockChance;}
	void					Set_AI_Special_Damage_State_Lock_Chance(float chance);
	bool					Is_AI_Attack_Wander_Enabled(void) const {return AIEnableAttackWander;}
	void					Set_AI_Attack_Wander_Enabled(bool enabled);
	bool					Is_AI_Damage_Retarget_Enabled(void) const {return AIEnableDamageRetarget;}
	void					Set_AI_Damage_Retarget_Enabled(bool enabled);
	bool					Are_AI_Unit_Combat_Types_Enabled(void) const {return AIEnableUnitCombatTypes;}
	void					Set_AI_Unit_Combat_Types_Enabled(bool enabled);

private:
	StringClass			Player2Preset;
	int					DifficultyLevel;
	bool					EnableSprint;
	bool					DisableHealthPickups;
	bool					DisableArmorPickups;
	bool					DisableAmmoPickups;
	float					EnemyHealthMultiplier;
	float					EnemyDamageMultiplier;
	int					DeathScorePenalty;
	float					AISightMultiplier;
	float					AIHearingMultiplier;
	float					AIAggressivenessBonus;
	float					AITakeCoverBonus;
	float					AIShareInfoRadius;
	float					AIWeaponErrorMultiplier;
	float					AISpecialDamageStateLockChance;
	bool					AIEnableAttackWander;
	bool					AIEnableDamageRetarget;
	bool					AIEnableUnitCombatTypes;
};

#endif // GDCOOPMISSION_H
