/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "gdcoopmission.h"

#include "assets.h"
#include "combat.h"
#include "gametype.h"
#include "ini.h"
#include "networkobject.h"
#include "playertype.h"
#include "wwpacket.h"
#include "wwdebug.h"

static const char *DEFAULT_COOP_PLAYER2_PRESET = "GDI_Logan_Sheppard_Tutorial";
static const int DEFAULT_COOP_MAX_PLAYERS = 4;
static const float DEFAULT_COOP_ENEMY_HEALTH_MULTIPLIER = 1.0f;
static const float DEFAULT_COOP_ENEMY_DAMAGE_MULTIPLIER = 1.0f;
static const int DEFAULT_COOP_DEATH_SCORE_PENALTY = 100;

//-----------------------------------------------------------------------------
static int Clamp_Coop_Max_Players(int max_players)
{
	const int max_network_players = NetworkObjectClass::MAX_CLIENT_COUNT - 1;
	if (max_players < 1) {
		return 1;
	}
	if (max_players > max_network_players) {
		return max_network_players;
	}

	return max_players;
}

//-----------------------------------------------------------------------------
static float Clamp_Coop_Percent_Multiplier(float multiplier, float min_multiplier)
{
	if (multiplier < min_multiplier) {
		return min_multiplier;
	}
	if (multiplier > 10.0f) {
		return 10.0f;
	}

	return multiplier;
}

//-----------------------------------------------------------------------------
static int Clamp_Coop_Death_Score_Penalty(int penalty)
{
	if (penalty < 0) {
		return 0;
	}
	if (penalty > 100000) {
		return 100000;
	}

	return penalty;
}

//-----------------------------------------------------------------------------
cGameDataCoopMission::cGameDataCoopMission(void) :
	cGameData(),
	Player2Preset(DEFAULT_COOP_PLAYER2_PRESET),
	DifficultyLevel(CombatManager::Get_Difficulty_Level()),
	EnableSprint(true),
	DisableHealthPickups(false),
	DisableArmorPickups(false),
	DisableAmmoPickups(false),
	EnemyHealthMultiplier(DEFAULT_COOP_ENEMY_HEALTH_MULTIPLIER),
	EnemyDamageMultiplier(DEFAULT_COOP_ENEMY_DAMAGE_MULTIPLIER),
	DeathScorePenalty(DEFAULT_COOP_DEATH_SCORE_PENALTY)
{
	Apply_Global_Settings();
	Set_Ini_Filename("svrcfg_coop.ini");
	Set_Ip_And_Port();
	Set_Game_Title(U_CHAR("Co-op Campaign"));
	Set_Max_Players(DEFAULT_COOP_MAX_PLAYERS);
	Set_Intermission_Time_Seconds(0);
	Set_Time_Limit_Minutes(0);
	Set_Map_Name("M01.mix");
	Set_Map_Cycle(0, "M01.mix");
	IsTeamChangingAllowed.Set(false);
	RemixTeams.Set(false);
	IsClanGame.Set(false);
	IsLaddered.Set(false);
	Set_QuickMatch_Server(false);
	Set_Radar_Mode(RADAR_ALL);
}

//-----------------------------------------------------------------------------
cGameDataCoopMission::~cGameDataCoopMission(void)
{
}

//-----------------------------------------------------------------------------
const unichar_t* cGameDataCoopMission::Get_Static_Game_Name(void)
{
	return U_CHAR("Co-op Campaign");
}

//-----------------------------------------------------------------------------
int cGameDataCoopMission::Choose_Player_Type(cPlayer* /* player */, int /* team_choice */, bool is_grunt)
{
	if (is_grunt) {
		return PLAYERTYPE_NOD;
	}

	return PLAYERTYPE_GDI;
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Difficulty_Level(int level)
{
	if (level < 0) {
		level = 0;
	} else if (level > 2) {
		level = 2;
	}

	DifficultyLevel = level;
	CombatManager::Set_Difficulty_Level(DifficultyLevel);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Sprint_Enabled(bool enabled)
{
	EnableSprint = enabled;
	cGameType::Set_Coop_Sprint_Enabled(EnableSprint);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Health_Pickups_Disabled(bool disabled)
{
	DisableHealthPickups = disabled;
	cGameType::Set_Coop_Health_Pickups_Disabled(DisableHealthPickups);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Armor_Pickups_Disabled(bool disabled)
{
	DisableArmorPickups = disabled;
	cGameType::Set_Coop_Armor_Pickups_Disabled(DisableArmorPickups);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Ammo_Pickups_Disabled(bool disabled)
{
	DisableAmmoPickups = disabled;
	cGameType::Set_Coop_Ammo_Pickups_Disabled(DisableAmmoPickups);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Enemy_Health_Multiplier(float multiplier)
{
	EnemyHealthMultiplier = Clamp_Coop_Percent_Multiplier(multiplier, 0.01f);
	cGameType::Set_Coop_Enemy_Health_Multiplier(EnemyHealthMultiplier);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Enemy_Damage_Multiplier(float multiplier)
{
	EnemyDamageMultiplier = Clamp_Coop_Percent_Multiplier(multiplier, 0.0f);
	cGameType::Set_Coop_Enemy_Damage_Multiplier(EnemyDamageMultiplier);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Death_Score_Penalty(int penalty)
{
	DeathScorePenalty = Clamp_Coop_Death_Score_Penalty(penalty);
	cGameType::Set_Coop_Death_Score_Penalty(DeathScorePenalty);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Apply_Global_Settings(void) const
{
	CombatManager::Set_Difficulty_Level(DifficultyLevel);
	cGameType::Set_Coop_Sprint_Enabled(EnableSprint);
	cGameType::Set_Coop_Health_Pickups_Disabled(DisableHealthPickups);
	cGameType::Set_Coop_Armor_Pickups_Disabled(DisableArmorPickups);
	cGameType::Set_Coop_Ammo_Pickups_Disabled(DisableAmmoPickups);
	cGameType::Set_Coop_Enemy_Health_Multiplier(EnemyHealthMultiplier);
	cGameType::Set_Coop_Enemy_Damage_Multiplier(EnemyDamageMultiplier);
	cGameType::Set_Coop_Death_Score_Penalty(DeathScorePenalty);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Load_From_Server_Config(void)
{
	cGameData::Load_From_Server_Config(Get_Ini_Filename());

	INIClass * p_ini = Get_INI(Get_Ini_Filename());
	WWASSERT(p_ini != NULL);

	int max_players = p_ini->Get_Int(INI_SECTION_NAME, "MaxPlayers", Get_Max_Players());
	Set_Max_Players(Clamp_Coop_Max_Players(max_players));

	int difficulty = p_ini->Get_Int(INI_SECTION_NAME, "Difficulty", DifficultyLevel);
	Set_Difficulty_Level(difficulty);

	char preset[256] = { 0 };
	p_ini->Get_String(INI_SECTION_NAME, "CoopPlayer2Preset", DEFAULT_COOP_PLAYER2_PRESET, preset, sizeof(preset));
	Player2Preset = preset;
	if (Player2Preset.Is_Empty()) {
		Player2Preset = DEFAULT_COOP_PLAYER2_PRESET;
	}

	bool friendly_fire = p_ini->Get_Bool(INI_SECTION_NAME, "IsFriendlyFirePermitted", IsFriendlyFirePermitted.Get());
	IsFriendlyFirePermitted.Set(friendly_fire);

	Set_Sprint_Enabled(p_ini->Get_Bool(INI_SECTION_NAME, "EnableSprint", EnableSprint));
	Set_Health_Pickups_Disabled(p_ini->Get_Bool(INI_SECTION_NAME, "DisableHealthPickups", DisableHealthPickups));
	Set_Armor_Pickups_Disabled(p_ini->Get_Bool(INI_SECTION_NAME, "DisableArmorPickups", DisableArmorPickups));
	Set_Ammo_Pickups_Disabled(p_ini->Get_Bool(INI_SECTION_NAME, "DisableAmmoPickups", DisableAmmoPickups));
	Set_Enemy_Health_Multiplier(p_ini->Get_Float(INI_SECTION_NAME, "EnemyHealthMultiplier", EnemyHealthMultiplier));
	Set_Enemy_Damage_Multiplier(p_ini->Get_Float(INI_SECTION_NAME, "EnemyDamageMultiplier", EnemyDamageMultiplier));
	Set_Death_Score_Penalty(p_ini->Get_Int(INI_SECTION_NAME, "DeathScorePenalty", DeathScorePenalty));

	bool maps_loop = p_ini->Get_Bool(INI_SECTION_NAME, "DoMapsLoop", false);
	Set_Do_Maps_Loop(maps_loop);

	IsTeamChangingAllowed.Set(false);
	RemixTeams.Set(false);
	IsClanGame.Set(false);
	IsLaddered.Set(false);
	Set_QuickMatch_Server(false);

	Release_INI(p_ini);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Save_To_Server_Config(void)
{
	cGameData::Save_To_Server_Config(Get_Ini_Filename());

	INIClass * p_ini = Get_INI(Get_Ini_Filename());
	WWASSERT(p_ini != NULL);

	p_ini->Put_Int(INI_SECTION_NAME, "MaxPlayers", Get_Max_Players());
	p_ini->Put_Int(INI_SECTION_NAME, "Difficulty", DifficultyLevel);
	p_ini->Put_String(INI_SECTION_NAME, "CoopPlayer2Preset", Player2Preset.Is_Empty() ? "" : Player2Preset.Peek_Buffer());
	p_ini->Put_Bool(INI_SECTION_NAME, "IsFriendlyFirePermitted", IsFriendlyFirePermitted.Get());
	p_ini->Put_Bool(INI_SECTION_NAME, "EnableSprint", EnableSprint);
	p_ini->Put_Bool(INI_SECTION_NAME, "DisableHealthPickups", DisableHealthPickups);
	p_ini->Put_Bool(INI_SECTION_NAME, "DisableArmorPickups", DisableArmorPickups);
	p_ini->Put_Bool(INI_SECTION_NAME, "DisableAmmoPickups", DisableAmmoPickups);
	p_ini->Put_Float(INI_SECTION_NAME, "EnemyHealthMultiplier", EnemyHealthMultiplier);
	p_ini->Put_Float(INI_SECTION_NAME, "EnemyDamageMultiplier", EnemyDamageMultiplier);
	p_ini->Put_Int(INI_SECTION_NAME, "DeathScorePenalty", DeathScorePenalty);
	p_ini->Put_Bool(INI_SECTION_NAME, "DoMapsLoop", Do_Maps_Loop());

	Save_INI(p_ini, Get_Ini_Filename());
	Release_INI(p_ini);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Export_Tier_2_Data(cPacket & packet)
{
	cGameData::Export_Tier_2_Data(packet);

	packet.Add(DifficultyLevel);
	packet.Add_Terminated_String(Player2Preset.Is_Empty() ? "" : Player2Preset.Peek_Buffer(), true);
	packet.Add(EnableSprint);
	packet.Add(DisableHealthPickups);
	packet.Add(DisableArmorPickups);
	packet.Add(DisableAmmoPickups);
	packet.Add(EnemyHealthMultiplier);
	packet.Add(EnemyDamageMultiplier);
	packet.Add(DeathScorePenalty);
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Import_Tier_2_Data(cPacket & packet)
{
	cGameData::Import_Tier_2_Data(packet);

	int difficulty = packet.Get(difficulty);
	Set_Difficulty_Level(difficulty);

	char preset[256] = { 0 };
	packet.Get_Terminated_String(preset, sizeof(preset), true);
	Player2Preset = preset;

	bool enable_sprint = packet.Get(enable_sprint);
	Set_Sprint_Enabled(enable_sprint);

	bool disable_health_pickups = packet.Get(disable_health_pickups);
	Set_Health_Pickups_Disabled(disable_health_pickups);

	bool disable_armor_pickups = packet.Get(disable_armor_pickups);
	Set_Armor_Pickups_Disabled(disable_armor_pickups);

	bool disable_ammo_pickups = packet.Get(disable_ammo_pickups);
	Set_Ammo_Pickups_Disabled(disable_ammo_pickups);

	float enemy_health_multiplier = packet.Get(enemy_health_multiplier);
	Set_Enemy_Health_Multiplier(enemy_health_multiplier);

	float enemy_damage_multiplier = packet.Get(enemy_damage_multiplier);
	Set_Enemy_Damage_Multiplier(enemy_damage_multiplier);

	int death_score_penalty = packet.Get(death_score_penalty);
	Set_Death_Score_Penalty(death_score_penalty);
}
