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
#include "ini.h"
#include "playertype.h"
#include "wwpacket.h"
#include "wwdebug.h"

static const char *DEFAULT_COOP_PLAYER2_PRESET = "GDI_Logan_Sheppard_Tutorial";

//-----------------------------------------------------------------------------
cGameDataCoopMission::cGameDataCoopMission(void) :
	cGameData(),
	Player2Preset(DEFAULT_COOP_PLAYER2_PRESET),
	DifficultyLevel(CombatManager::Get_Difficulty_Level())
{
	Set_Ini_Filename("svrcfg_coop.ini");
	Set_Ip_And_Port();
	Set_Game_Title(U_CHAR("Co-op Campaign"));
	Set_Max_Players(2);
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
void cGameDataCoopMission::Load_From_Server_Config(void)
{
	cGameData::Load_From_Server_Config(Get_Ini_Filename());

	INIClass * p_ini = Get_INI(Get_Ini_Filename());
	WWASSERT(p_ini != NULL);

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

	bool maps_loop = p_ini->Get_Bool(INI_SECTION_NAME, "DoMapsLoop", false);
	Set_Do_Maps_Loop(maps_loop);

	Set_Max_Players(2);
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

	p_ini->Put_Int(INI_SECTION_NAME, "MaxPlayers", 2);
	p_ini->Put_Int(INI_SECTION_NAME, "Difficulty", DifficultyLevel);
	p_ini->Put_String(INI_SECTION_NAME, "CoopPlayer2Preset", Player2Preset.Is_Empty() ? "" : Player2Preset.Peek_Buffer());
	p_ini->Put_Bool(INI_SECTION_NAME, "IsFriendlyFirePermitted", IsFriendlyFirePermitted.Get());
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
}
