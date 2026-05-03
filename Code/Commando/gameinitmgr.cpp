/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Combat																		  *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/Commando/gameinitmgr.cpp       $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 6/21/02 2:24p                                               $*
 *                                                                                             *
 *                    $Revision:: 75                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "gameinitmgr.h"

#include <cstring>

#include "gamedata.h"
#include "gamemode.h"
#include "cnetwork.h"
#include "ww3d.h"
#include "singlepl.h"
#include "player.h"
#include "renegadedialogmgr.h"
#include "netinterface.h"
#include "langmode.h"
#include "wolgmode.h"
#include "playermanager.h"
#include "WWAudio.h"
#include "saveloadstatus.h"
#include "combatgmode.h"
#include "useroptions.h"
#include "lanchat.h"
#include "netutil.h"
#include "rendobj.h"
#include "phys.h"
#include "pscene.h"
#include "dx8renderer.h"
#include "gdsingleplayer.h"
#include "gdcoopmission.h"
#include "gdskirmish.h"
#include "playertype.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "god.h"
#include "bioevent.h"
#include "devoptions.h"
#include "svrgoodbyeevent.h"
#include	"natter.h"
#include "apppacketstats.h"
#include "packetmgr.h"
#include "AutoStart.h"
#include "wwmemlog.h"
#include "gamesideservercontrol.h"
#include "slavemaster.h"
#include "hud.h"
#include "gamespyadmin.h"
#include "ServerSettings.h"
#include "GameSpy_QnR.h"
#include "ConsoleMode.h"
#include "specialbuilds.h"
#include "modpackagemgr.h"
#include "teammanager.h"
#include "campaign.h"

#include "translatedb.h"
#include "damage.h"
#include "ccamera.h"
#include "coopdebuglog.h"
#include "cooplobbymgr.h"
#include "bones.h"
#include "surfaceeffects.h"
#include "ffactory.h"
#include "ini.h"
#include "dazzle.h"
#include "scriptman.h"



static void _reload_game_configuration_files(void);

// Defines.
#define PRE_SERVICE_TIME	1500 // Time in milliseconds.
#define POST_SERVICE_TIME	 250 // Time in milliseconds.


////////////////////////////////////////////////////////////////
//	Static member initialization
////////////////////////////////////////////////////////////////
bool		GameInitMgrClass::IsClientRequired	= false;
bool		GameInitMgrClass::IsServerRequired	= false;
bool		GameInitMgrClass::RestoreSFX			= false;
bool		GameInitMgrClass::RestoreMusic		= false;
bool		GameInitMgrClass::NeedsGameExit		= false;
bool		GameInitMgrClass::NeedsGameExitAll	= false;
bool		GameInitMgrClass::IsCoopLevelTransition = false;
bool		GameInitMgrClass::HasPendingCoopLevelTransition = false;
char		GameInitMgrClass::PendingCoopLevelTransitionMap[MAX_MAPNAME_SIZE] = { 0 };
int		GameInitMgrClass::PendingCoopLevelTransitionDifficulty = 0;
int		GameInitMgrClass::Mode					= MODE_UNKNOWN;
int		GameInitMgrClass::WOLReturnDialog	= RenegadeDialogMgrClass::LOC_INTERNET_MAIN;


bool GameInitMgrClass::Is_Game_In_Progress(void)
{
	GameModeClass* mode = GameModeManager::Find("Combat");
	//return (mode && mode->Is_Active());
	return (mode != NULL && !mode->Is_Inactive());
}


////////////////////////////////////////////////////////////////
//
//	Start_Game
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Start_Game (const char *map_name, int teamChoice, unsigned int clanID)
{
	unsigned int time;

	WWASSERT(map_name != NULL);
   WWDEBUG_SAY (("GameInitMgrClass::Start_Game(%s)\n", map_name));
	CoopDebugLog::Log("GameInitMgrClass::Start_Game begin map=%s mode=%d teamChoice=%d client_required=%d server_required=%d game_type=%d",
		map_name, Mode, teamChoice, IsClientRequired, IsServerRequired, cGameType::Get_Game_Type());

	// NOTE: Multi-play does not need this fix because it does not sound page swap.
	if (IS_SOLOPLAY) {

		// IML: First, allow a short period to process any outstanding sound effects that may have
		// been started by the caller.
		time = TIMEGETTIME();
		while (TIMEGETTIME() - time < PRE_SERVICE_TIME) {
			WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		}

 		// IML: Ensure that there are no sound effects lingering on any playlist.
		WWAudioClass::Get_Instance ()->Flush_Playlist();

		// IML: Allow audio system to clean-up after flush.
		time = TIMEGETTIME();
		while (TIMEGETTIME() - time < POST_SERVICE_TIME) {
			WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		}
	}

	//
	// Kill off any old suspended game
	//
	if (GameModeManager::Find ("Combat")->Is_Suspended ()) {
		End_Game ();
		GameModeManager::Safely_Deactivate ();
	}

	//
	//	Set the map name
	//
	StringClass map(map_name,true);
	WWASSERT(PTheGameData != NULL);
	The_Game ()->Set_Map_Name (map);
	if (IS_COOP_MISSION) {
		cGameDataCoopMission *coop_game = The_Game()->As_Coop_Mission();
		if (coop_game != NULL) {
			coop_game->Apply_Global_Settings();
		}
	}

	//
	//	Determine if there is a mod specified... if so, load the mod package
	//
	ModPackageMgrClass::Load_Current_Mod ();
	CoopDebugLog::Log("GameInitMgrClass::Start_Game loaded mod package map=%s", map_name);

	//
	// Reload the sub-systems that may be affected by a mod
	//
	_reload_game_configuration_files();
	CoopDebugLog::Log("GameInitMgrClass::Start_Game reloaded configuration map=%s", map_name);

	//
	//	Check to ensure the game is configured correctly
	//
	#ifdef WWDEBUG
	WideStringClass outMsg;

	if (!The_Game()->Is_Valid_Settings(outMsg)) {
		WWDEBUG_SAY(("ERROR: %S\n", (const unichar_t*)outMsg));
		WWASSERT("The_Game()->Is_Valid_Settings()");
	}
	#endif

	//
	// Reset Data Safe state.
	//
	GenericDataSafeClass::Reset();

	//
	// Reset packet optimizer bandwidth stats.
	//
	PacketManager.Reset_Stats();

	//
	//	Start either the client or server (or both) depending
	// on which mode we are in.
	//
	Start_Client_Server ();
	CoopDebugLog::Log("GameInitMgrClass::Start_Game Start_Client_Server done map=%s", map_name);

	//
	//	Deactivate the menu system
	//
	INIT_STATUS ("Deactivate menu");
	GameModeManager::Find ("Menu")->Deactivate ();

	//
	//	Active the combat system
	//
	INIT_STATUS ("Activate combat");
   GameModeManager::Find ("Combat")->Activate ();
	INIT_STATUS ("");
	CoopDebugLog::Log("GameInitMgrClass::Start_Game combat activated map=%s", map_name);

	//
	//	Load the level
	//
	CombatGameModeClass *game_mode = static_cast<CombatGameModeClass*>(GameModeManager::Find ("Combat"));
	CoopDebugLog::Log("GameInitMgrClass::Start_Game Load_Level start map=%s", map_name);
	game_mode->Load_Level ();
	CoopDebugLog::Log("GameInitMgrClass::Start_Game Load_Level done map=%s", map_name);

   //
	//	Let the LAN or WOL interface know we are starting a game
	//
	if (Mode == MODE_LAN || Mode == MODE_COOP_LAN) {
		INIT_STATUS ("Go to location");
		PLC->Go_To_Location (LANLOC_INGAME);
		CoopDebugLog::Log("GameInitMgrClass::Start_Game LAN location set map=%s", map_name);
	} else if (Mode == MODE_WOL) {
		INIT_STATUS ("Go to game channel");
	}

	//
	//	Reset some rendering data
	//
	PhysicsSceneClass::Get_Instance()->Release_Projector_Resources ();
	TheDX8MeshRenderer.Invalidate ();

	//
	// Prevent the first couple frames from rendering, so that all textures get cached.
	//
	GameModeManager::Hide_Render_Frames (2);

	//
	//	Send team/player information to the server (if necessary)
	//
	Transmit_Player_Data (teamChoice, clanID);

	//
	// Set the auto restart flag if required.
	//
	AutoRestart.Set_Restart_Flag((The_Game()->IsAutoRestart.Is_True()) ? true : false);

	//
	// Listen for server control messages.
	//
	GameSideServerControlClass::Init();

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Start_Coop_Lobby
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Start_Coop_Lobby(const char *map_name, int difficulty_level)
{
	WWASSERT(map_name != NULL);
	WWASSERT(IS_COOP_MISSION);

	if (map_name == NULL || map_name[0] == 0) {
		return;
	}

	if (!IsClientRequired) {
		CampaignManager::Start_Coop_Campaign(map_name, difficulty_level);
		return;
	}

	CoopDebugLog::Log("GameInitMgrClass::Start_Coop_Lobby begin map=%s difficulty=%d client_required=%d server_required=%d",
		map_name, difficulty_level, IsClientRequired, IsServerRequired);

	StringClass map(map_name, true);
	WWASSERT(PTheGameData != NULL);
	The_Game()->Set_Map_Name(map);

	cGameDataCoopMission *coop_game = The_Game()->As_Coop_Mission();
	if (coop_game != NULL) {
		coop_game->Set_Difficulty_Level(difficulty_level);
		coop_game->Apply_Global_Settings();
	}

	Start_Client_Server();
	CoopDebugLog::Log("GameInitMgrClass::Start_Coop_Lobby Start_Client_Server done map=%s", map_name);
	Transmit_Player_Data(PLAYERTYPE_GDI, 0);

	GameModeManager::Find("Menu")->Deactivate();

	if (Mode == MODE_LAN || Mode == MODE_COOP_LAN) {
		PLC->Go_To_Location(LANLOC_LOBBY);
	}

	if (cNetwork::I_Am_Server()) {
		CoopLobbyMgrClass::Open_Pre_Game(map_name, difficulty_level);
	} else if (!CoopLobbyMgrClass::Is_Active()) {
		CoopLobbyMgrClass::Open_Client_Pre_Game(map_name, difficulty_level);
	}
}


////////////////////////////////////////////////////////////////
//
//	End_Game
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::End_Game (void)
{
	unsigned int time;

	WWDEBUG_SAY (("GameInitMgrClass::End_Game\n"));
	CoopDebugLog::Log("GameInitMgrClass::End_Game begin mode=%d game_type=%d in_progress=%d",
		Mode, cGameType::Get_Game_Type(), Is_Game_In_Progress());

	// Do nothing if the game is not in progress.
	if ( !IS_MISSION && (!Is_Game_In_Progress())) {
		CoopDebugLog::Log("GameInitMgrClass::End_Game ignored because game is not in progress");
		return;
	}

	// NOTE: Multi-play does not need this fix because it does not sound page swap.
	if (IS_SOLOPLAY) {

		// IML: Allow a short period to process any outstanding sound effects that may have
		// been started by the caller.
		time = TIMEGETTIME();
		while (TIMEGETTIME() - time < PRE_SERVICE_TIME) {
			WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		}

		// IML: Ensure that there are no sound effects lingering on any playlist.
		WWAudioClass::Get_Instance ()->Flush_Playlist();

		// IML: Allow audio system to clean-up after flush.
		time = TIMEGETTIME();
		while (TIMEGETTIME() - time < POST_SERVICE_TIME) {
			WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		}
	}

#ifndef MULTIPLAYERDEMO
	if ( IS_MISSION && PTheGameData != NULL && The_Game()->Remember_Inventory() && COMBAT_STAR ) {
		cGod::Store_Inventory( COMBAT_STAR );
	}
#endif // !MULTIPLAYERDEMO

	// Stop reporting to Gamespy.
	if (Mode == MODE_LAN && GameSpyQnR.IsEnabled()) {
		GameSpyQnR.Shutdown();
	}

	//
	// A dedicated server will disable sfx & music. Restore them here.
	//
	if (WWAudioClass::Get_Instance() != NULL) {
		if (RestoreSFX) {
			WWAudioClass::Get_Instance()->Allow_Sound_Effects(true);
		}
		if (RestoreMusic) {
			WWAudioClass::Get_Instance()->Allow_Music(true);
		}
	}

   if (GameModeManager::Find( "Combat" )->Is_Active()) {
      //
      // Combat is still active during the ingame menu and multiplayer gameplay.
      // Suspend it first because that is the state that it is in for
      // single-player.
      //
      GameModeManager::Find( "Combat" )->Suspend();
   }

	//
	//	Notify the game data object that the game is over
	//
	cGameData* theGame = PTheGameData;

	//
	// Shut down slave game servers.
	//
	SlaveMaster.Shutdown_Slaves();

	//
	// Stop listening for server control messages.
	//
	GameSideServerControlClass::Shutdown();

	if (theGame) {
		theGame->On_Game_End();
	}

	//
	// Disable auto restart mode.
	//
	// For forced exits the mode will already be correct.
	//
	if (!cGameData::Is_Manual_Exit()) {
		AutoRestart.Set_Restart_Flag(false);
	}

	//
	//	Shutdown the combat system
	//
	GameModeManager::Find ("Combat")->Deactivate ();
	CoopDebugLog::Log("GameInitMgrClass::End_Game combat deactivated");

	//
	//	Let the game mode manager think to cleanup all pending states
	//
	GameModeManager::Think ();
	CoopDebugLog::Log("GameInitMgrClass::End_Game GameModeManager cleanup think done");

 	//
	//	Shutdown the menu system as necessary
	//
	if (GameModeManager::Find ("Menu")->Is_Active ()) {
		GameModeManager::Find ("Menu")->Deactivate ();
	}

	// Leave the WWOnline game.
	GameModeClass* gameMode = GameModeManager::Find("WOL");

	if (gameMode && gameMode->Is_Active()) {
		WolGameModeClass* wolGame = static_cast<WolGameModeClass*>(gameMode);
		WWASSERT(wolGame != NULL);
		wolGame->Leave_Game();
	}

	bool preserve_network = IsCoopLevelTransition && IS_COOP_MISSION;

	if (cNetwork::I_Am_Server() && !preserve_network) {

		bool is_quick_full_exit_requested = false;
#ifdef WWDEBUG
		is_quick_full_exit_requested = cDevOptions::QuickFullExit.Get();
#endif // WWDEBUG

// FIXME (TSS) ***** Memory leak here - please fix (ST - 6/14/2001 2:06PM) *****
		cSvrGoodbyeEvent * p_event = new cSvrGoodbyeEvent;
		p_event->Init(is_quick_full_exit_requested);
	}

	cNetwork::Flush();
	CoopDebugLog::Log("GameInitMgrClass::End_Game cNetwork::Flush done preserve_network=%d", preserve_network);

	if (!preserve_network) {
		CoopDebugLog::Log("GameInitMgrClass::End_Game End_Client_Server start");
		End_Client_Server();
		CoopDebugLog::Log("GameInitMgrClass::End_Game End_Client_Server done");
	}

	//
	//	Remove all players
	//
	cPlayerManager::Remove_All();
	cTeamManager::Remove_All();

	//
	// TSS092301
	// Destroy all netobjects !
	//
	NetworkObjectMgrClass::Set_All_Delete_Pending();
	CoopDebugLog::Log("GameInitMgrClass::End_Game Set_All_Delete_Pending done pending=%d",
		NetworkObjectMgrClass::Get_Pending_Object_Count());
	NetworkObjectMgrClass::Delete_Pending();
	CoopDebugLog::Log("GameInitMgrClass::End_Game Delete_Pending done objects=%d",
		NetworkObjectMgrClass::Get_Object_Count());

	cGod::Reset();

	//
	//	Unload whatever mod is currently loaded (if necessary)...
	//
	ModPackageMgrClass::Unload_Current_Mod ();
	CoopDebugLog::Log("GameInitMgrClass::End_Game done");
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Continue_Game
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Continue_Game(void)
{
	unsigned int time;

	// IML : First, allow a short period to process any outstanding sound effects that may have been started by the caller.
	// NOTE: Multi-play does not need this fix because it does not sound page swap.
	if (IS_SOLOPLAY) {
		time = TIMEGETTIME();
		while (TIMEGETTIME() - time < PRE_SERVICE_TIME) {
			WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		}
	}

	GameModeManager::Find ("Menu")->Deactivate ();
	GameModeManager::Find ("Combat" )->Resume ();

	//Force the hud to rebuild, in case weapn chart text changed
	HUDClass::Force_Weapon_Chart_Update();

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Display_End_Game_Menu
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Display_End_Game_Menu (void)
{
	switch (Mode)
	{
		//
		//	Display the main menu
		//
		default:
		case MODE_SP:
		case MODE_SKIRMISH:
			RenegadeDialogMgrClass::Goto_Location (RenegadeDialogMgrClass::LOC_MAIN_MENU);
			break;

		//
		//	Display the LAN main menu
		//
		case MODE_LAN:
		case MODE_COOP_LAN:
			//GAMESPY
			if (cGameSpyAdmin::Is_Gamespy_Game()) {
				RenegadeDialogMgrClass::Goto_Location (RenegadeDialogMgrClass::LOC_GAMESPY_MAIN);
			} else {
				RenegadeDialogMgrClass::Goto_Location (RenegadeDialogMgrClass::LOC_LAN_MAIN);
			}
			break;

		//
		//	Display the WOL main menu
		//
		case MODE_WOL:
			RenegadeDialogMgrClass::Goto_Location ((RenegadeDialogMgrClass::LOCATION)WOLReturnDialog);
			break;
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Transmit_Player_Data
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Transmit_Player_Data (int teamChoice, unsigned int clanID)
{
	WWMEMLOG(MEM_NETWORK);
   WWDEBUG_SAY (("GameInitMgrClass::Transmit_Player_Data\n"));

	if (Mode == MODE_SP || Mode == MODE_SKIRMISH) {

		//
		//	Send generic team information for the client in a single player game
		//
		cBioEvent * p_event = new cBioEvent;
		p_event->Init(teamChoice, clanID);

	} else if (IsClientRequired) {

		//
		//	Send the player's team choice to the server
		//
		cBioEvent * p_event = new cBioEvent;
		p_event->Init(teamChoice, clanID);
	}

   WWDEBUG_SAY (("GameInitMgrClass::Transmit_Player_Data Done\n"));
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Start_Client_Server
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Start_Client_Server (void)
{
   WWDEBUG_SAY (("GameInitMgrClass::Start_Client_Server\n"));

	assert(GameModeManager::Find("WOL"));
		if (GameModeManager::Find("WOL")->Is_Active()) {
			if (PTheGameData != NULL) {
				const unsigned short wol_port = WOLNATInterface.Get_Port_As_Server();
				if (wol_port >= MIN_SERVER_PORT && wol_port <= MAX_SERVER_PORT) {
					The_Game()->Set_Port(wol_port);
			} else {
				WWDEBUG_SAY(("WOL port %hu outside valid range, keeping existing game port\n", wol_port));
			}
		}
		} else if (GameModeManager::Find("LAN")->Is_Active() && cGameSpyAdmin::Is_Gamespy_Game()) {
			if (PTheGameData != NULL) {
				const int gamespy_port = cUserOptions::GameSpyGamePort.Get();
				if (gamespy_port >= MIN_SERVER_PORT && gamespy_port <= MAX_SERVER_PORT) {
					The_Game()->Set_Port(gamespy_port);
					ConsoleBox.Print("GameSpy server port set to %d\n", gamespy_port);
				} else {
					WWDEBUG_SAY(("GameSpy port %d outside valid range, keeping existing game port\n", gamespy_port));
				}
			}
		}

#ifdef WWDEBUG
	cRemoteHost::Set_Allow_Extra_Modem_Bandwidth_Throttling(cDevOptions::ExtraModemBandwidthThrottling.Get());
#endif //WWDEBUG

	//
	//	Start the server (if necessary)
	//
	if (IsServerRequired && !cNetwork::I_Am_Server ()) {
		cNetwork::Init_Server ();
		PacketManager.Set_Is_Server(true);

		//
		// Dedicated server disables playing of sfx & music
		//
		if ((IsClientRequired == false) && WWAudioClass::Get_Instance () != NULL) {

			if (WWAudioClass::Get_Instance ()->Are_Sound_Effects_On ()) {
				WWAudioClass::Get_Instance ()->Allow_Sound_Effects (false);
				RestoreSFX = true;
			}

			if (WWAudioClass::Get_Instance ()->Is_Music_On ()) {
				WWAudioClass::Get_Instance ()->Allow_Music (false);
				RestoreMusic = true;
			}
		}
	}
	if (IsServerRequired && cNetwork::I_Am_Server () &&
		 cTeamManager::Get_Team_Object_List()->Head() == NULL) {
		for (int team_num = 0; team_num < MAX_TEAMS; team_num++) {
			cTeam * p_team = new cTeam;
			p_team->Init(team_num);
		}
	}

	//
	//	Start the client (if necessary)
	//
	if (IsClientRequired && !cNetwork::I_Am_Client ()) {

		if (!IsServerRequired) {
			PacketManager.Set_Is_Server(false);
		}

		assert(GameModeManager::Find("WOL"));
		if (GameModeManager::Find("WOL")->Is_Active()) {
			const unsigned short wol_client_port = WOLNATInterface.Get_Port_As_Server_Client();
			if (wol_client_port >= MIN_SERVER_PORT && wol_client_port <= MAX_SERVER_PORT) {
				cNetwork::Init_Client(wol_client_port);
			} else {
				WWDEBUG_SAY(("WOL client port %hu outside valid range, falling back to default client init\n", wol_client_port));
				cNetwork::Init_Client();
			}
		} else {
			cNetwork::Init_Client();
		}

		//
		//	Wait for the client to connect to the server
		//
		WWDEBUG_SAY(("BEFORE GameInitMgrClass::Start_Client_Server tight update loop\n"));
		WWDEBUG_SAY(("Game IP = %s\n", cNetUtil::Address_To_String(The_Game()->Get_Ip_Address())));
		unsigned int time = TIMEGETTIME();
		do {
			cNetwork::Update ();
			if (TIMEGETTIME() - time > 20*1000) {
				break;
			}
		} while (!cNetwork::PClientConnection->Is_Established ());
		WWDEBUG_SAY(("AFTER GameInitMgrClass::Start_Client_Server tight update loop\n"));
	}

	// Sample output every 2 seconds.
	PacketManager.Set_Stats_Sampling_Frequency_Delay(2000);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	End_Client_Server
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::End_Client_Server (void)
{
   WWDEBUG_SAY (("GameInitMgrClass::End_Client_Server\n"));

	//
	//	Cleanup the client
	//
	if (cNetwork::I_Am_Client ()) {
		cNetwork::Cleanup_Client ();
	}

	//
	//	Cleanup the server
	//
	if (cNetwork::I_Am_Server ()) {
		cNetwork::Cleanup_Server ();
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Initialize_SP
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Initialize_SP (void)
{
#ifndef MULTIPLAYERDEMO

   WWDEBUG_SAY (("GameInitMgrClass::Initialize_SP\n"));

	if (Mode != MODE_UNKNOWN) {
		Shutdown ();
	}

	//
	// Notify combat
	//
	//cSingleData::Set_Is_Single_Player (true);
	cGameType::Set_Game_Type(GAMETYPE_MISSION);

	//
	// Notify wwnet
	//
	cSinglePlayerData::Init ();

	WideStringClass widestring;
	widestring.Convert_From("Renegade");
   cNetInterface::Set_Nickname(widestring);

	//
	//	Create the new game type
	//
	WWASSERT (PTheGameData == NULL);
	PTheGameData = new cGameDataSinglePlayer;
	WWASSERT(PTheGameData != NULL);

	//
	//	Remember our state
	//
	IsClientRequired	= true;
	IsServerRequired	= true;
	Mode					= MODE_SP;
	return ;

#endif // !MULTIPLAYERDEMO
}


////////////////////////////////////////////////////////////////
//
//	Shutdown_SP
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Shutdown_SP (void)
{
#ifndef MULTIPLAYERDEMO

   WWDEBUG_SAY (("GameInitMgrClass::Shutdown_SP\n"));

//// FIXME TSS Fix memory leak here

	//cSingleData::Set_Is_Single_Player(false);
	cGameType::Set_Game_Type(GAMETYPE_NONE);

#endif // !MULTIPLAYERDEMO
}


////////////////////////////////////////////////////////////////
//
//	Initialize_Skirmish
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Initialize_Skirmish(void)
{
#ifndef MULTIPLAYERDEMO

   WWDEBUG_SAY(("GameInitMgrClass::Initialize_Skirmish\n"));

	if (Mode != MODE_UNKNOWN) {
		Shutdown ();
	}

	//
	// Notify combat
	//
	//cSingleData::Set_Is_Single_Player(true);
	cGameType::Set_Game_Type(GAMETYPE_SKIRMISH);

	//
	//	Notify wwnet
	//
	cSinglePlayerData::Init ();

	WideStringClass widestring;
	widestring.Convert_From("Renegade");
   cNetInterface::Set_Nickname(widestring);

	//
	//	Create the new game type
	//
	WWASSERT (PTheGameData == NULL);
	PTheGameData = new cGameDataSkirmish;
	WWASSERT(PTheGameData != NULL);

	//
	//	Remember our state
	//
	IsClientRequired	= true;
	IsServerRequired	= true;
	Mode					= MODE_SKIRMISH;

#endif // !MULTIPLAYERDEMO
}


////////////////////////////////////////////////////////////////
//
//	Shutdown_Skirmish
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Shutdown_Skirmish(void)
{
#ifndef MULTIPLAYERDEMO

   WWDEBUG_SAY(("GameInitMgrClass::Shutdown_Skirmish\n"));

	//cSingleData::Set_Is_Single_Player(false);
	cGameType::Set_Game_Type(GAMETYPE_NONE);

#endif // !MULTIPLAYERDEMO
}


////////////////////////////////////////////////////////////////
//
//	Initialize_LAN
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Initialize_LAN (void)
{
   WWDEBUG_SAY (("GameInitMgrClass::Initialize_LAN\n"));

	if (Mode != MODE_UNKNOWN) {
		Shutdown ();
	}

	//cSingleData::Set_Is_Single_Player (false);
	cGameType::Set_Game_Type(GAMETYPE_MULTIPLAY);

	//
	// Activate LAN interface
	//
	GameModeManager::Find ("LAN")->Activate ();

	//
	//	Remember our state
	//
	IsClientRequired	= false;
	IsServerRequired	= false;
	Mode					= MODE_LAN;

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Initialize_Coop_LAN
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Initialize_Coop_LAN (void)
{
	WWDEBUG_SAY (("GameInitMgrClass::Initialize_Coop_LAN\n"));
	CoopDebugLog::Reset();
	CoopDebugLog::Log("GameInitMgrClass::Initialize_Coop_LAN begin mode=%d", Mode);

	if (Mode != MODE_UNKNOWN) {
		CoopDebugLog::Log("GameInitMgrClass::Initialize_Coop_LAN shutting down existing mode=%d", Mode);
		Shutdown ();
	}

	cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
	CoopDebugLog::Log("GameInitMgrClass::Initialize_Coop_LAN game type set to coop mission");

	//
	// Activate LAN interface. Co-op uses real LAN sockets, not single-player queues.
	//
	GameModeManager::Find ("LAN")->Activate ();

	IsClientRequired	= false;
	IsServerRequired	= false;
	Mode					= MODE_COOP_LAN;
	CoopDebugLog::Log("GameInitMgrClass::Initialize_Coop_LAN done mode=%d", Mode);

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Shutdown_LAN
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Shutdown_LAN (void)
{
   WWDEBUG_SAY (("GameInitMgrClass::Shutdown_LAN\n"));

   //
	//	Deactive the LAN interface
	//
	GameModeManager::Find ("LAN")->Deactivate ();

	cGameType::Set_Game_Type(GAMETYPE_NONE);
}


////////////////////////////////////////////////////////////////
//
//	Initialize_WOL
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Initialize_WOL (void)
{
#ifndef MULTIPLAYERDEMO

#ifdef _WIN64
	// WOLAPI COM binaries are 32-bit only; bail out early in 64-bit builds.
	ConsoleBox.Print("Westwood Online is not available in 64-bit builds. Returning to main menu.\n");
	RenegadeDialogMgrClass::Goto_Location (RenegadeDialogMgrClass::LOC_MAIN_MENU);
	cGameType::Set_Game_Type(GAMETYPE_NONE);
	Mode = MODE_UNKNOWN;
	return;
#endif

	WWDEBUG_SAY (("GameInitMgrClass::Initialize_WOL\n"));

	if (Mode != MODE_UNKNOWN) {
		Shutdown ();
	}

	//cSingleData::Set_Is_Single_Player (false);
	cGameType::Set_Game_Type(GAMETYPE_MULTIPLAY);

	//
	// Activate WOL interface
	//
	GameModeManager::Find ("WOL")->Activate ();

	//
	//	Remember our state
	//
	IsClientRequired	= false;
	IsServerRequired	= false;
	Mode					= MODE_WOL;
	return ;

#endif // !MULTIPLAYERDEMO
}


////////////////////////////////////////////////////////////////
//
//	Shutdown_WOL
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Shutdown_WOL (void)
{
#ifndef MULTIPLAYERDEMO

   WWDEBUG_SAY (("GameInitMgrClass::Shutdown_WOL\n"));

	GameModeManager::Find ("WOL")->Deactivate ();

	cGameType::Set_Game_Type(GAMETYPE_NONE);

#endif // !MULTIPLAYERDEMO
}


////////////////////////////////////////////////////////////////
//
//	Shutdown
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Shutdown (void)
{
   WWDEBUG_SAY (("GameInitMgrClass::Shutdown\n"));

	switch (Mode)
	{
		case MODE_SP:
			Shutdown_SP ();
			break;

		case MODE_SKIRMISH:
			Shutdown_Skirmish ();
			break;

		case MODE_LAN:
		case MODE_COOP_LAN:
			Shutdown_LAN ();
			break;

		case MODE_WOL:
			Shutdown_WOL ();
			break;
	}

	//
	//	Reset our state
	//
	IsClientRequired	= false;
	IsServerRequired	= false;
	Mode					= MODE_UNKNOWN;

	End_Client_Server();

	if (cSinglePlayerData::Is_Single_Player()) {
		//
		// This needs to be done after the Cleanup_Client and Cleanup_Server
		//
		cSinglePlayerData::Cleanup();
	}

	//
	//	Free the old game data
	//
	if (PTheGameData != NULL) {
		delete PTheGameData;
		PTheGameData = NULL;
	}

	//
	//	Reset state
	//
	IsClientRequired	= false;
	IsServerRequired	= false;
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Queue_Coop_Level_Transition
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Queue_Coop_Level_Transition(const char *map_name, int difficulty_level)
{
	if (map_name == NULL || map_name[0] == 0) {
		return;
	}

	::strncpy(PendingCoopLevelTransitionMap, map_name, sizeof(PendingCoopLevelTransitionMap) - 1);
	PendingCoopLevelTransitionMap[sizeof(PendingCoopLevelTransitionMap) - 1] = 0;
	PendingCoopLevelTransitionDifficulty = difficulty_level;
	HasPendingCoopLevelTransition = true;

	CoopDebugLog::Log("GameInitMgrClass::Queue_Coop_Level_Transition map=%s difficulty=%d",
		PendingCoopLevelTransitionMap, PendingCoopLevelTransitionDifficulty);
}


////////////////////////////////////////////////////////////////
//
//	Think
//
////////////////////////////////////////////////////////////////
void
GameInitMgrClass::Think (void)
{
	if (HasPendingCoopLevelTransition) {
		char map_name[MAX_MAPNAME_SIZE];
		::strncpy(map_name, PendingCoopLevelTransitionMap, sizeof(map_name) - 1);
		map_name[sizeof(map_name) - 1] = 0;
		int difficulty_level = PendingCoopLevelTransitionDifficulty;

		HasPendingCoopLevelTransition = false;
		PendingCoopLevelTransitionMap[0] = 0;

		CoopDebugLog::Log("GameInitMgrClass::Think handling co-op level transition map=%s difficulty=%d",
			map_name, difficulty_level);

		if (IS_COOP_MISSION && map_name[0] != 0) {
			CampaignManager::Prepare_Coop_Campaign_Level(map_name, difficulty_level);

			if (PTheGameData != NULL) {
				StringClass map(map_name, true);
				The_Game()->Set_Map_Name(map);

				cGameDataCoopMission *coop_game = The_Game()->As_Coop_Mission();
				if (coop_game != NULL) {
					coop_game->Set_Difficulty_Level(difficulty_level);
					coop_game->Apply_Global_Settings();
				}
				The_Game()->IsIntermission.Set(false);
			}

			GameModeClass *combat_mode = GameModeManager::Find("Combat");
			if (combat_mode != NULL && !combat_mode->Is_Inactive()) {
				extern bool g_b_core_restart;
				g_b_core_restart = true;
				CoopDebugLog::Log("GameInitMgrClass::Think queued co-op core restart map=%s difficulty=%d",
					map_name, difficulty_level);
			} else {
				CoopDebugLog::Log("GameInitMgrClass::Think starting co-op campaign directly map=%s difficulty=%d",
					map_name, difficulty_level);
				CampaignManager::Start_Coop_Campaign(map_name, difficulty_level);
			}
			return;
		}
	}

	//
	//	Safely exit the game and return to the menu (as necessary)
	//
	if (NeedsGameExit) {
		CoopDebugLog::Log("GameInitMgrClass::Think handling NeedsGameExit");
		GameInitMgrClass::End_Game ();
		GameInitMgrClass::Display_End_Game_Menu ();
		NeedsGameExit = false;
	}

	if (NeedsGameExitAll) {
		CoopDebugLog::Log("GameInitMgrClass::Think handling NeedsGameExitAll");
		GameInitMgrClass::End_Game ();
		extern void Stop_Main_Loop (int exitCode);
		Stop_Main_Loop (EXIT_SUCCESS);
	}

	return ;
}




void _reload_game_configuration_files(void)
{
	//
	// (gth) All of this stuff is part of the day 120 Renegade patch and
	// is designed to make it possible to change the following systems
	// in mods
	//
	ArmorWarheadManager::Init();
	BonesManager::Init();
	SurfaceEffectsManager::Init();

#if 0 // re-initting cameras seems to cause problems...
	CCameraClass::Init();
#endif

	// Reload dazzles
	FileClass * dazzle_ini_file = _TheFileFactory->Get_File("DAZZLE.INI");
	if (dazzle_ini_file) {
		INIClass dazzle_ini(*dazzle_ini_file);
		DazzleRenderObjClass::Init_From_INI(&dazzle_ini);
		_TheFileFactory->Return_File(dazzle_ini_file);
	}

	//	Reload the strings table
	TranslateDBClass::Initialize();
	FileClass *file	= _TheFileFactory->Get_File( "STRINGS.TDB" );
	if (file != NULL) {
		file->Open (FileClass::READ);				//	Open or the file
		if ( file->Is_Available() ) {
			ChunkLoadClass cload (file);				// Load the database
			SaveLoadSystemClass::Load(cload);
		}
		file->Close ();								// Close the file
		_TheFileFactory->Return_File (file);
	}

	// Reload scripts.dll
	ScriptManager::Destroy_Pending();
	ScriptManager::Shutdown();
	ScriptManager::Init();
}
