/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPLOBBYMGR_H__
#define __COOPLOBBYMGR_H__

#include "gamedata.h"
#include "wwstring.h"
#include "widestring.h"

class BitStreamClass;
class cGameDataCoopMission;

class CoopLobbyMgrClass
{
public:
	enum Phase
	{
		PHASE_INACTIVE = 0,
		PHASE_PRE_GAME,
		PHASE_BETWEEN_LEVELS,
		PHASE_STARTING_MAP
	};

	enum
	{
		MAX_PLAYERS = 16,
		MAX_CHAT_MESSAGES = 32,
		MAX_CHAT_MESSAGE_LENGTH = 256
	};

	struct HostOptions
	{
		int DifficultyLevel;
		bool EnableSprint;
		bool DisableHealthPickups;
		bool DisableArmorPickups;
		bool DisableAmmoPickups;
		float EnemyHealthMultiplier;
		float EnemyDamageMultiplier;
		int DeathScorePenalty;
		float AISightMultiplier;
		float AIHearingMultiplier;
		float AIAggressivenessBonus;
		float AITakeCoverBonus;
		float AIShareInfoRadius;
		float AIWeaponErrorMultiplier;
		float AISpecialDamageStateLockChance;
		bool AIEnableAttackWander;
		bool AIEnableDamageRetarget;
		bool AIEnableUnitCombatTypes;
	};

	struct MissionResults
	{
		bool HasResults;
		char MapName[MAX_MAPNAME_SIZE];
		int DifficultyLevel;
		float CompletionTime;
		int SecondaryObjectives;
		int CompletedSecondaryObjectives;
		int TertiaryObjectives;
		int CompletedTertiaryObjectives;
		int TeamDeaths;
		int TimeStars;
		int DifficultyStars;
		int SecondaryStars;
		int SurvivalStars;
		int OverallStars;
	};

	struct PlayerStats
	{
		int PlayerId;
		WideStringClass Name;
		bool Ready;
		int Score;
		int Kills;
		int Deaths;
		int Ping;
		float GameTime;
		int EnemiesKilled;
		int AlliesKilled;
		int ShotsFired;
		int HeadShots;
		int TorsoShots;
		int ArmShots;
		int LegShots;
		int CrotchShots;
		int Powerups;
		int VehiclesDestroyed;
		float VehicleTime;
		int VehicleKills;
		int Squishes;
		int BuildingsDestroyed;
		float CreditsGranted;
		float Accuracy;
	};

	static bool Is_Active(void) { return LobbyPhase != PHASE_INACTIVE; }
	static Phase Get_Phase(void) { return LobbyPhase; }
	static const char *Get_Pending_Map(void) { return PendingMap; }
	static int Get_Pending_Difficulty(void) { return PendingDifficulty; }
	static const HostOptions &Get_Host_Options(void) { return Options; }
	static const MissionResults &Get_Mission_Results(void) { return Results; }
	static const PlayerStats *Get_Player_Stats(void) { return Players; }
	static int Get_Player_Count(void) { return PlayerCount; }
	static const WideStringClass *Get_Chat_Log(void) { return ChatLog; }
	static int Get_Chat_Count(void) { return ChatCount; }
	static bool Get_Local_Ready(void) { return LocalReady; }

	static void Open_Pre_Game(const char *map_name, int difficulty_level);
	static void Open_Client_Pre_Game(const char *map_name, int difficulty_level);
	static void Open_Between_Levels(const char *next_map_name, int difficulty_level);
	static void Start_Pending_Map(void);
	static void Close(void);

	static void Toggle_Local_Ready(void);
	static void Set_Player_Ready(int player_id, bool ready);
	static void Add_Chat_Message(const WideStringClass &message);
	static void Send_Chat_Message(WideStringClass &message);
	static void Refresh_Player_List(void);

	static void Broadcast_State(void);
	static void Send_State_To_Client(int client_id);
	static void Export_State(BitStreamClass &packet);
	static void Import_State(BitStreamClass &packet);

private:
	static void Reset_State(void);
	static void Open_Lobby(Phase phase, const char *map_name, int difficulty_level, bool capture_results, bool broadcast);
	static void Capture_Host_Options(void);
	static void Capture_Host_Options(const cGameDataCoopMission *coop_game);
	static void Capture_Mission_Results(void);
	static int Get_Time_Stars(const char *map_name, float play_time);
	static int Get_Difficulty_Stars(int difficulty_level);
	static int Get_Objective_Stars(int completed, int total);
	static int Get_Survival_Stars(int team_deaths);
	static void Show_Dialog(void);
	static void Close_Dialog(void);
	static int Find_Player_Row(int player_id);

	static Phase LobbyPhase;
	static char PendingMap[MAX_MAPNAME_SIZE];
	static int PendingDifficulty;
	static HostOptions Options;
	static MissionResults Results;
	static PlayerStats Players[MAX_PLAYERS];
	static int PlayerCount;
	static WideStringClass ChatLog[MAX_CHAT_MESSAGES];
	static int ChatCount;
	static bool LocalReady;
};

#endif // __COOPLOBBYMGR_H__
