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
cGameDataCoopMission::cGameDataCoopMission(void) :
	cGameData(),
	Player2Preset(DEFAULT_COOP_PLAYER2_PRESET),
	DifficultyLevel(CombatManager::Get_Difficulty_Level()),
	EnableSprint(true)
{
	cGameType::Set_Coop_Sprint_Enabled(EnableSprint);
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
}

//-----------------------------------------------------------------------------
void cGameDataCoopMission::Set_Sprint_Enabled(bool enabled)
{
	EnableSprint = enabled;
	cGameType::Set_Coop_Sprint_Enabled(EnableSprint);
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
}
