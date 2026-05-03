/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "cooplobbymgr.h"

#include <cstring>

#include "apppackettypes.h"
#include "bitstream.h"
#include "campaign.h"
#include "cnetwork.h"
#include "coopdebuglog.h"
#include "coopleveltransitionevent.h"
#include "cooplobbyreadyevent.h"
#include "cooplobbystateevent.h"
#include "cstextobj.h"
#include "dialogmgr.h"
#include "dlgcooplobby.h"
#include "gameinitmgr.h"
#include "gamemode.h"
#include "gdcoopmission.h"
#include "objectives.h"
#include "player.h"
#include "playermanager.h"
#include "renegadedialog.h"
#include "sctextobj.h"

static const int CoopLobbyLevelStarTimes[] = {
	10, 20, 40, 20, 25, 30, 20, 35, 20, 25, 35, 25, 10, 10
};

CoopLobbyMgrClass::Phase CoopLobbyMgrClass::LobbyPhase = CoopLobbyMgrClass::PHASE_INACTIVE;
char CoopLobbyMgrClass::PendingMap[MAX_MAPNAME_SIZE] = { 0 };
int CoopLobbyMgrClass::PendingDifficulty = 0;
CoopLobbyMgrClass::HostOptions CoopLobbyMgrClass::Options = { 0 };
CoopLobbyMgrClass::MissionResults CoopLobbyMgrClass::Results = { 0 };
CoopLobbyMgrClass::PlayerStats CoopLobbyMgrClass::Players[CoopLobbyMgrClass::MAX_PLAYERS];
int CoopLobbyMgrClass::PlayerCount = 0;
WideStringClass CoopLobbyMgrClass::ChatLog[CoopLobbyMgrClass::MAX_CHAT_MESSAGES];
int CoopLobbyMgrClass::ChatCount = 0;
bool CoopLobbyMgrClass::LocalReady = false;

void CoopLobbyMgrClass::Reset_State(void)
{
	LobbyPhase = PHASE_INACTIVE;
	PendingMap[0] = 0;
	PendingDifficulty = 0;
	std::memset(&Options, 0, sizeof(Options));
	std::memset(&Results, 0, sizeof(Results));
	PlayerCount = 0;
	ChatCount = 0;
	LocalReady = false;
}

void CoopLobbyMgrClass::Open_Pre_Game(const char *map_name, int difficulty_level)
{
	Open_Lobby(PHASE_PRE_GAME, map_name, difficulty_level, false, cNetwork::I_Am_Server());
}

void CoopLobbyMgrClass::Open_Client_Pre_Game(const char *map_name, int difficulty_level)
{
	Open_Lobby(PHASE_PRE_GAME, map_name, difficulty_level, false, false);
}

void CoopLobbyMgrClass::Open_Between_Levels(const char *next_map_name, int difficulty_level)
{
	Open_Lobby(PHASE_BETWEEN_LEVELS, next_map_name, difficulty_level, true, cNetwork::I_Am_Server());
}

void CoopLobbyMgrClass::Open_Lobby(Phase phase, const char *map_name, int difficulty_level, bool capture_results, bool broadcast)
{
	if (map_name == NULL || map_name[0] == 0) {
		return;
	}

	LobbyPhase = phase;
	std::strncpy(PendingMap, map_name, sizeof(PendingMap) - 1);
	PendingMap[sizeof(PendingMap) - 1] = 0;
	PendingDifficulty = difficulty_level;
	LocalReady = false;
	ChatCount = 0;

	Capture_Host_Options();
	if (capture_results) {
		Capture_Mission_Results();
	} else {
		std::memset(&Results, 0, sizeof(Results));
	}

	Refresh_Player_List();
	Show_Dialog();

	if (broadcast) {
		Broadcast_State();
	}

	CoopDebugLog::Log("CoopLobbyMgrClass::Open_Lobby phase=%d map=%s difficulty=%d",
		LobbyPhase, PendingMap, PendingDifficulty);
}

void CoopLobbyMgrClass::Start_Pending_Map(void)
{
	if (!cNetwork::I_Am_Server() || LobbyPhase == PHASE_INACTIVE || PendingMap[0] == 0) {
		return;
	}

	char map_name[MAX_MAPNAME_SIZE];
	std::strncpy(map_name, PendingMap, sizeof(map_name) - 1);
	map_name[sizeof(map_name) - 1] = 0;
	int difficulty_level = PendingDifficulty;

	LobbyPhase = PHASE_STARTING_MAP;
	Broadcast_State();

	cCoopLevelTransitionEvent *transition_event = new cCoopLevelTransitionEvent;
	transition_event->Init(map_name, difficulty_level);
	cNetwork::Flush();

	Close();
	GameInitMgrClass::Queue_Coop_Level_Transition(map_name, difficulty_level);
}

void CoopLobbyMgrClass::Close(void)
{
	Close_Dialog();
	Reset_State();
}

void CoopLobbyMgrClass::Toggle_Local_Ready(void)
{
	LocalReady = !LocalReady;

	if (cNetwork::I_Am_Server()) {
		Set_Player_Ready(cNetwork::Get_My_Id(), LocalReady);
		Broadcast_State();
	} else if (cNetwork::I_Am_Client()) {
		cCoopLobbyReadyEvent *event = new cCoopLobbyReadyEvent;
		event->Init(LocalReady);
	}
}

void CoopLobbyMgrClass::Set_Player_Ready(int player_id, bool ready)
{
	int index = Find_Player_Row(player_id);
	if (index < 0) {
		Refresh_Player_List();
		index = Find_Player_Row(player_id);
	}

	if (index >= 0) {
		Players[index].Ready = ready;
	}

	if (cNetwork::I_Am_Client() && player_id == cNetwork::Get_My_Id()) {
		LocalReady = ready;
	}
}

void CoopLobbyMgrClass::Add_Chat_Message(const WideStringClass &message)
{
	if (!Is_Active() || message.Is_Empty()) {
		return;
	}

	if (ChatCount >= MAX_CHAT_MESSAGES) {
		for (int index = 1; index < MAX_CHAT_MESSAGES; index++) {
			ChatLog[index - 1] = ChatLog[index];
		}
		ChatCount = MAX_CHAT_MESSAGES - 1;
	}

	ChatLog[ChatCount++] = message;
}

void CoopLobbyMgrClass::Send_Chat_Message(WideStringClass &message)
{
	message.Trim();
	if (message.Is_Empty()) {
		return;
	}

	if (cNetwork::I_Am_Client()) {
		cCsTextObj *event_obj = new cCsTextObj;
		event_obj->Init(message, TEXT_MESSAGE_PUBLIC, cNetwork::Get_My_Id(), -1);
	} else if (cNetwork::I_Am_Server()) {
		cScTextObj *event_obj = new cScTextObj;
		event_obj->Init(message, TEXT_MESSAGE_PUBLIC, false, HOST_TEXT_SENDER, -1);
	}
}

void CoopLobbyMgrClass::Refresh_Player_List(void)
{
	bool old_ready[MAX_PLAYERS];
	int old_ids[MAX_PLAYERS];
	int old_count = PlayerCount;
	for (int index = 0; index < old_count; index++) {
		old_ids[index] = Players[index].PlayerId;
		old_ready[index] = Players[index].Ready;
	}

	PlayerCount = 0;
	SList<cPlayer> *player_list = cPlayerManager::Get_Player_Object_List();
	if (player_list == NULL) {
		return;
	}

	for (SLNode<cPlayer> *node = player_list->Head();
		node != NULL && PlayerCount < MAX_PLAYERS;
		node = node->Next()) {
		cPlayer *player = node->Data();
		if (player == NULL || !player->Is_Active()) {
			continue;
		}

		PlayerStats &stats = Players[PlayerCount++];
		stats.PlayerId = player->Get_Id();
		stats.Name = player->Get_Name();
		stats.Ready = false;
		for (int ready_index = 0; ready_index < old_count; ready_index++) {
			if (old_ids[ready_index] == stats.PlayerId) {
				stats.Ready = old_ready[ready_index];
				break;
			}
		}
		stats.Score = (int)player->Get_Score();
		stats.Kills = player->Get_Kills();
		stats.Deaths = player->Get_Deaths();
		stats.Ping = player->Get_Ping();
		stats.GameTime = player->Get_Game_Time();
		stats.EnemiesKilled = player->Get_Enemies_Killed();
		stats.AlliesKilled = player->Get_Allies_Killed();
		stats.ShotsFired = player->Get_Shots_Fired();
		stats.HeadShots = player->Get_Head_Shots();
		stats.TorsoShots = player->Get_Torso_Shots();
		stats.ArmShots = player->Get_Arm_Shots();
		stats.LegShots = player->Get_Leg_Shots();
		stats.CrotchShots = player->Get_Crotch_Shots();
		stats.Powerups = player->Get_Powerups_Collected();
		stats.VehiclesDestroyed = player->Get_Vehiclies_Destroyed();
		stats.VehicleTime = player->Get_Vehicle_Time();
		stats.VehicleKills = player->Get_Kills_From_Vehicle();
		stats.Squishes = player->Get_Squishes();
		stats.BuildingsDestroyed = player->Get_Building_Destroyed();
		stats.CreditsGranted = player->Get_Credit_Grant();

		int hits = stats.HeadShots + stats.TorsoShots + stats.ArmShots + stats.LegShots + stats.CrotchShots;
		stats.Accuracy = stats.ShotsFired > 0 ? (hits * 100.0f) / (float)stats.ShotsFired : 100.0f;
	}
}

void CoopLobbyMgrClass::Broadcast_State(void)
{
	if (!cNetwork::I_Am_Server() || !Is_Active()) {
		return;
	}

	Refresh_Player_List();

	cCoopLobbyStateEvent *event = new cCoopLobbyStateEvent;
	event->Init(-1);
}

void CoopLobbyMgrClass::Send_State_To_Client(int client_id)
{
	if (!cNetwork::I_Am_Server() || !Is_Active() || client_id <= 0) {
		return;
	}

	Refresh_Player_List();

	cCoopLobbyStateEvent *event = new cCoopLobbyStateEvent;
	event->Init(client_id);
}

void CoopLobbyMgrClass::Export_State(BitStreamClass &packet)
{
	packet.Add((int)LobbyPhase);
	packet.Add_Terminated_String(PendingMap, true);
	packet.Add(PendingDifficulty);

	packet.Add(Options.DifficultyLevel);
	packet.Add(Options.EnableSprint);
	packet.Add(Options.DisableHealthPickups);
	packet.Add(Options.DisableArmorPickups);
	packet.Add(Options.DisableAmmoPickups);
	packet.Add(Options.EnemyHealthMultiplier);
	packet.Add(Options.EnemyDamageMultiplier);
	packet.Add(Options.DeathScorePenalty);
	packet.Add(Options.AISightMultiplier);
	packet.Add(Options.AIHearingMultiplier);
	packet.Add(Options.AIAggressivenessBonus);
	packet.Add(Options.AITakeCoverBonus);
	packet.Add(Options.AIShareInfoRadius);
	packet.Add(Options.AIWeaponErrorMultiplier);
	packet.Add(Options.AISpecialDamageStateLockChance);
	packet.Add(Options.AIEnableAttackWander);
	packet.Add(Options.AIEnableDamageRetarget);
	packet.Add(Options.AIEnableUnitCombatTypes);

	packet.Add(Results.HasResults);
	packet.Add_Terminated_String(Results.MapName, true);
	packet.Add(Results.DifficultyLevel);
	packet.Add(Results.CompletionTime);
	packet.Add(Results.SecondaryObjectives);
	packet.Add(Results.CompletedSecondaryObjectives);
	packet.Add(Results.TertiaryObjectives);
	packet.Add(Results.CompletedTertiaryObjectives);
	packet.Add(Results.TeamDeaths);
	packet.Add(Results.TimeStars);
	packet.Add(Results.DifficultyStars);
	packet.Add(Results.SecondaryStars);
	packet.Add(Results.SurvivalStars);
	packet.Add(Results.OverallStars);

	packet.Add(PlayerCount);
	for (int index = 0; index < PlayerCount; index++) {
		PlayerStats &stats = Players[index];
		packet.Add(stats.PlayerId);
		packet.Add_Wide_Terminated_String(stats.Name);
		packet.Add(stats.Ready);
		packet.Add(stats.Score);
		packet.Add(stats.Kills);
		packet.Add(stats.Deaths);
		packet.Add(stats.Ping);
		packet.Add(stats.GameTime);
		packet.Add(stats.EnemiesKilled);
		packet.Add(stats.AlliesKilled);
		packet.Add(stats.ShotsFired);
		packet.Add(stats.HeadShots);
		packet.Add(stats.TorsoShots);
		packet.Add(stats.ArmShots);
		packet.Add(stats.LegShots);
		packet.Add(stats.CrotchShots);
		packet.Add(stats.Powerups);
		packet.Add(stats.VehiclesDestroyed);
		packet.Add(stats.VehicleTime);
		packet.Add(stats.VehicleKills);
		packet.Add(stats.Squishes);
		packet.Add(stats.BuildingsDestroyed);
		packet.Add(stats.CreditsGranted);
		packet.Add(stats.Accuracy);
	}

	packet.Add(ChatCount);
	for (int chat_index = 0; chat_index < ChatCount; chat_index++) {
		packet.Add_Wide_Terminated_String(ChatLog[chat_index]);
	}
}

void CoopLobbyMgrClass::Import_State(BitStreamClass &packet)
{
	int phase = 0;
	packet.Get(phase);
	LobbyPhase = (Phase)phase;

	packet.Get_Terminated_String(PendingMap, sizeof(PendingMap), true);
	packet.Get(PendingDifficulty);

	packet.Get(Options.DifficultyLevel);
	packet.Get(Options.EnableSprint);
	packet.Get(Options.DisableHealthPickups);
	packet.Get(Options.DisableArmorPickups);
	packet.Get(Options.DisableAmmoPickups);
	packet.Get(Options.EnemyHealthMultiplier);
	packet.Get(Options.EnemyDamageMultiplier);
	packet.Get(Options.DeathScorePenalty);
	packet.Get(Options.AISightMultiplier);
	packet.Get(Options.AIHearingMultiplier);
	packet.Get(Options.AIAggressivenessBonus);
	packet.Get(Options.AITakeCoverBonus);
	packet.Get(Options.AIShareInfoRadius);
	packet.Get(Options.AIWeaponErrorMultiplier);
	packet.Get(Options.AISpecialDamageStateLockChance);
	packet.Get(Options.AIEnableAttackWander);
	packet.Get(Options.AIEnableDamageRetarget);
	packet.Get(Options.AIEnableUnitCombatTypes);

	packet.Get(Results.HasResults);
	packet.Get_Terminated_String(Results.MapName, sizeof(Results.MapName), true);
	packet.Get(Results.DifficultyLevel);
	packet.Get(Results.CompletionTime);
	packet.Get(Results.SecondaryObjectives);
	packet.Get(Results.CompletedSecondaryObjectives);
	packet.Get(Results.TertiaryObjectives);
	packet.Get(Results.CompletedTertiaryObjectives);
	packet.Get(Results.TeamDeaths);
	packet.Get(Results.TimeStars);
	packet.Get(Results.DifficultyStars);
	packet.Get(Results.SecondaryStars);
	packet.Get(Results.SurvivalStars);
	packet.Get(Results.OverallStars);

	packet.Get(PlayerCount);
	if (PlayerCount < 0) {
		PlayerCount = 0;
	}
	if (PlayerCount > MAX_PLAYERS) {
		PlayerCount = MAX_PLAYERS;
	}

	for (int index = 0; index < PlayerCount; index++) {
		PlayerStats &stats = Players[index];
		packet.Get(stats.PlayerId);
		packet.Get_Wide_Terminated_String(stats.Name.Get_Buffer(128), 128);
		packet.Get(stats.Ready);
		packet.Get(stats.Score);
		packet.Get(stats.Kills);
		packet.Get(stats.Deaths);
		packet.Get(stats.Ping);
		packet.Get(stats.GameTime);
		packet.Get(stats.EnemiesKilled);
		packet.Get(stats.AlliesKilled);
		packet.Get(stats.ShotsFired);
		packet.Get(stats.HeadShots);
		packet.Get(stats.TorsoShots);
		packet.Get(stats.ArmShots);
		packet.Get(stats.LegShots);
		packet.Get(stats.CrotchShots);
		packet.Get(stats.Powerups);
		packet.Get(stats.VehiclesDestroyed);
		packet.Get(stats.VehicleTime);
		packet.Get(stats.VehicleKills);
		packet.Get(stats.Squishes);
		packet.Get(stats.BuildingsDestroyed);
		packet.Get(stats.CreditsGranted);
		packet.Get(stats.Accuracy);

		if (cNetwork::I_Am_Client() && stats.PlayerId == cNetwork::Get_My_Id()) {
			LocalReady = stats.Ready;
		}
	}

	packet.Get(ChatCount);
	if (ChatCount < 0) {
		ChatCount = 0;
	}
	if (ChatCount > MAX_CHAT_MESSAGES) {
		ChatCount = MAX_CHAT_MESSAGES;
	}

	for (int chat_index = 0; chat_index < ChatCount; chat_index++) {
		packet.Get_Wide_Terminated_String(ChatLog[chat_index].Get_Buffer(MAX_CHAT_MESSAGE_LENGTH), MAX_CHAT_MESSAGE_LENGTH);
	}

	if (LobbyPhase == PHASE_INACTIVE) {
		Close_Dialog();
	} else {
		Show_Dialog();
	}
}

void CoopLobbyMgrClass::Capture_Host_Options(void)
{
	Capture_Host_Options(The_Game() != NULL ? The_Game()->As_Coop_Mission() : NULL);
}

void CoopLobbyMgrClass::Capture_Host_Options(const cGameDataCoopMission *coop_game)
{
	if (coop_game == NULL) {
		std::memset(&Options, 0, sizeof(Options));
		Options.EnableSprint = true;
		Options.EnemyHealthMultiplier = 1.0f;
		Options.EnemyDamageMultiplier = 1.0f;
		return;
	}

	Options.DifficultyLevel = coop_game->Get_Difficulty_Level();
	Options.EnableSprint = coop_game->Is_Sprint_Enabled();
	Options.DisableHealthPickups = coop_game->Are_Health_Pickups_Disabled();
	Options.DisableArmorPickups = coop_game->Are_Armor_Pickups_Disabled();
	Options.DisableAmmoPickups = coop_game->Are_Ammo_Pickups_Disabled();
	Options.EnemyHealthMultiplier = coop_game->Get_Enemy_Health_Multiplier();
	Options.EnemyDamageMultiplier = coop_game->Get_Enemy_Damage_Multiplier();
	Options.DeathScorePenalty = coop_game->Get_Death_Score_Penalty();
	Options.AISightMultiplier = coop_game->Get_AI_Sight_Multiplier();
	Options.AIHearingMultiplier = coop_game->Get_AI_Hearing_Multiplier();
	Options.AIAggressivenessBonus = coop_game->Get_AI_Aggressiveness_Bonus();
	Options.AITakeCoverBonus = coop_game->Get_AI_Take_Cover_Bonus();
	Options.AIShareInfoRadius = coop_game->Get_AI_Share_Info_Radius();
	Options.AIWeaponErrorMultiplier = coop_game->Get_AI_Weapon_Error_Multiplier();
	Options.AISpecialDamageStateLockChance = coop_game->Get_AI_Special_Damage_State_Lock_Chance();
	Options.AIEnableAttackWander = coop_game->Is_AI_Attack_Wander_Enabled();
	Options.AIEnableDamageRetarget = coop_game->Is_AI_Damage_Retarget_Enabled();
	Options.AIEnableUnitCombatTypes = coop_game->Are_AI_Unit_Combat_Types_Enabled();
}

void CoopLobbyMgrClass::Capture_Mission_Results(void)
{
	std::memset(&Results, 0, sizeof(Results));
	Results.HasResults = true;

	const char *map_name = The_Game() != NULL ? The_Game()->Get_Map_Name().Peek_Buffer() : "";
	std::strncpy(Results.MapName, map_name != NULL ? map_name : "", sizeof(Results.MapName) - 1);
	Results.MapName[sizeof(Results.MapName) - 1] = 0;
	Results.DifficultyLevel = Options.DifficultyLevel;

	Refresh_Player_List();

	float completion_time = 0.0f;
	int team_deaths = 0;
	for (int index = 0; index < PlayerCount; index++) {
		if (Players[index].GameTime > completion_time) {
			completion_time = Players[index].GameTime;
		}
		team_deaths += Players[index].Deaths;
	}

	Results.CompletionTime = completion_time;
	Results.TeamDeaths = team_deaths;
	Results.SecondaryObjectives = ObjectiveManager::Get_Num_Objectives(ObjectiveManager::TYPE_SECONDARY);
	Results.CompletedSecondaryObjectives = ObjectiveManager::Get_Num_Completed_Objectives(ObjectiveManager::TYPE_SECONDARY);
	Results.TertiaryObjectives = ObjectiveManager::Get_Num_Objectives(ObjectiveManager::TYPE_TERTIARY);
	Results.CompletedTertiaryObjectives = ObjectiveManager::Get_Num_Completed_Objectives(ObjectiveManager::TYPE_TERTIARY);
	Results.TimeStars = Get_Time_Stars(Results.MapName, Results.CompletionTime);
	Results.DifficultyStars = Get_Difficulty_Stars(Results.DifficultyLevel);
	Results.SecondaryStars = Get_Objective_Stars(Results.CompletedSecondaryObjectives, Results.SecondaryObjectives);
	Results.SurvivalStars = Get_Survival_Stars(Results.TeamDeaths);
	Results.OverallStars = (Results.TimeStars + Results.DifficultyStars + Results.SecondaryStars + Results.SurvivalStars) / 4;
}

int CoopLobbyMgrClass::Get_Time_Stars(const char *map_name, float play_time)
{
	int mission = cGameData::Get_Mission_Number_From_Map_Name(map_name != NULL ? map_name : "");
	if (mission < 0) {
		mission = 0;
	}
	if (mission >= (int)(sizeof(CoopLobbyLevelStarTimes) / sizeof(CoopLobbyLevelStarTimes[0]))) {
		mission = (int)(sizeof(CoopLobbyLevelStarTimes) / sizeof(CoopLobbyLevelStarTimes[0])) - 1;
	}

	float minutes = (play_time + 59.0f) / 60.0f;
	float par_time = (float)CoopLobbyLevelStarTimes[mission];

	int stars = 1;
	if (minutes <= par_time * 1.5f) stars = 2;
	if (minutes <= par_time * 1.3f) stars = 3;
	if (minutes <= par_time * 1.1f) stars = 4;
	if (minutes <= par_time) stars = 5;
	return stars;
}

int CoopLobbyMgrClass::Get_Difficulty_Stars(int difficulty_level)
{
	if (difficulty_level >= 2) {
		return 5;
	}
	if (difficulty_level >= 1) {
		return 3;
	}
	return 1;
}

int CoopLobbyMgrClass::Get_Objective_Stars(int completed, int total)
{
	float ratio = total > 0 ? (float)completed / (float)total : 1.0f;
	if (ratio >= 1.0f) return 5;
	if (ratio >= 0.9f) return 4;
	if (ratio >= 0.7f) return 3;
	if (ratio >= 0.5f) return 2;
	return 1;
}

int CoopLobbyMgrClass::Get_Survival_Stars(int team_deaths)
{
	if (team_deaths <= 0) return 5;
	if (team_deaths == 1) return 4;
	if (team_deaths <= 3) return 3;
	if (team_deaths <= 6) return 2;
	return 1;
}

void CoopLobbyMgrClass::Show_Dialog(void)
{
	if (DialogMgrClass::Find_Dialog((int)RenegadeDialogID::IDD_COOP_LOBBY) == NULL) {
		START_DIALOG(CoopLobbyDialogClass);
	}
}

void CoopLobbyMgrClass::Close_Dialog(void)
{
	DialogBaseClass *dialog = DialogMgrClass::Find_Dialog((int)RenegadeDialogID::IDD_COOP_LOBBY);
	if (dialog != NULL) {
		dialog->End_Dialog();
	}
}

int CoopLobbyMgrClass::Find_Player_Row(int player_id)
{
	for (int index = 0; index < PlayerCount; index++) {
		if (Players[index].PlayerId == player_id) {
			return index;
		}
	}

	return -1;
}
