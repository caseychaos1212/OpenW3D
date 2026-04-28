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

	const StringClass &	Get_Player2_Preset(void) const {return Player2Preset;}
	void					Set_Player2_Preset(const StringClass & preset) {Player2Preset = preset;}
	int					Get_Difficulty_Level(void) const {return DifficultyLevel;}
	void					Set_Difficulty_Level(int level);

private:
	StringClass			Player2Preset;
	int					DifficultyLevel;
};

#endif // GDCOOPMISSION_H
