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
 ***                            Confidential - Westwood Studios                              ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Commando                                                     *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/Commando/mainloop.cpp                        $*
 *                                                                                             *
 *                      $Author:: Tom_s                                                        $*
 *                                                                                             *
 *                     $Modtime:: 2/21/02 3:13p                                               $*
 *                                                                                             *
 *                    $Revision:: 77                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "mainloop.h"
#include "init.h"
#include "shutdown.h"
#include "timemgr.h"
#include "input.h"
#include "gamemode.h"
#include "debug.h"
#include "msgloop.h"
#include "wwprofile.h"
#include "cnetwork.h"
#include "coopdebuglog.h"
#include "miscutil.h"
//#include "gamesettings.h"
#include "WWAudio.h"
#include "devoptions.h"
#include "multihud.h"
#include "gamedata.h"
#include "diagnostics.h"
#include "wwprofile.h"
#include "crandom.h"
#include "dialogmgr.h"
#include "ccamera.h"
#include "pathmgr.h"
#include "networkobjectmgr.h"
#include "WebBrowser.h"
#include "AutoStart.h"
#include "gameinitmgr.h"
#include "servercontrol.h"
#include "ConsoleMode.h"
#include "gamespyadmin.h"
#include "demosupport.h"
#include "GameSpy_QnR.h"
#include "gametype.h"


/*
**
*/
bool	RunMainLoop = true;
int		ExitCode = EXIT_SUCCESS;

static bool Should_Log_Coop_Frame(void)
{
	static int coop_frame_log_budget = 1200;
	return IS_COOP_MISSION && cNetwork::I_Am_Client() && coop_frame_log_budget-- > 0;
}

void Stop_Main_Loop(int exitCode)
{
	CoopDebugLog::Log("Stop_Main_Loop exitCode=%d", exitCode);
	RunMainLoop = false;
	ExitCode = exitCode;
}


void _Game_Main_Loop_Loop(void)
{
	WWPROFILE( "Main Loop" );

	unsigned int time1 = TIMEGETTIME();
	const bool log_coop_frame = Should_Log_Coop_Frame();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop frame begin combat_active=%d net_objects=%d pending_deletes=%d",
			GameModeManager::Find("Combat") != NULL && GameModeManager::Find("Combat")->Is_Active(),
			NetworkObjectMgrClass::Get_Object_Count(),
			NetworkObjectMgrClass::Get_Pending_Object_Count());
	}

	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop TimeManager::Update start");
	}
   TimeManager::Update();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop TimeManager::Update done");
	}

	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop Input::Update start");
	}
   Input::Update();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop Input::Update done");
	}


{	WWPROFILE( "Pathfind Evaluate" );
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop pathfind start camera=%p", COMBAT_CAMERA);
	}
   if (COMBAT_CAMERA != NULL) {
		Vector3 camera_pos = COMBAT_CAMERA->Get_Position();
		PathMgrClass::Resolve_Paths( camera_pos );
	}
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop pathfind done");
	}
}

{	WWPROFILE( "Think" );
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop GameModeManager::Think start");
	}
   GameModeManager::Think();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop GameModeManager::Think done");
	}
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop GameInitMgrClass::Think start");
	}
	GameInitMgrClass::Think();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop GameInitMgrClass::Think done");
	}
}

{	WWPROFILE( "Dialog Mgr Update" );
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop DialogMgrClass::On_Frame_Update start");
	}
   DialogMgrClass::On_Frame_Update ();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop DialogMgrClass::On_Frame_Update done");
	}
}

{	WWPROFILE( "Network Object Mgr Think" );
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop NetworkObjectMgrClass::Think start objects=%d pending=%d",
			NetworkObjectMgrClass::Get_Object_Count(), NetworkObjectMgrClass::Get_Pending_Object_Count());
	}
   NetworkObjectMgrClass::Think ();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop NetworkObjectMgrClass::Think done objects=%d pending=%d",
			NetworkObjectMgrClass::Get_Object_Count(), NetworkObjectMgrClass::Get_Pending_Object_Count());
	}
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop ServerControl.Service start");
	}
	ServerControl.Service();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop ServerControl.Service done");
	}
}

{	WWPROFILE("GameSpy_QnR");
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop GameSpyQnR.Think start");
	}
	GameSpyQnR.Think();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop GameSpyQnR.Think done");
	}
}

	if (cGameSpyAdmin::Needs_Think()) {
		WWPROFILE( "cGameSpyAdmin Think" );
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop cGameSpyAdmin::Think start");
		}
		cGameSpyAdmin::Think();
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop cGameSpyAdmin::Think done");
		}
	}

	//
	// If the following assert hits it may indicate that your
	// working directory pathname got cleared in the project settings.
	//
	WWASSERT(GameModeManager::Find("Combat") != NULL);

	if (!GameModeManager::Find("Combat")->Is_Active()) {
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop inactive combat cNetwork::Update start");
		}
		cNetwork::Update();
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop inactive combat cNetwork::Update done");
		}
	}

	// Denzil - Embedded browser
#if WEBBROWSER_ENABLED
	if (WebBrowser::IsWebPageDisplayed() == false) {
#else
    {
#endif
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop GameModeManager::Render start");
		}
		GameModeManager::Render();
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop GameModeManager::Render done");
		}
	}

	if (AutoRestart.Is_Active()) {
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop AutoRestart.Think start");
		}
		AutoRestart.Think();
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop AutoRestart.Think done");
		}
	}

{	WWPROFILE("ConsoleBox");
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop ConsoleBox.Think start");
	}
	ConsoleBox.Think();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop ConsoleBox.Think done");
	}
}

	DEMO_SECURITY_CHECK;

{	WWPROFILE( "Audio" );
	if (!ConsoleBox.Is_Exclusive()) {
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop WWAudio On_Frame_Update start");
		}
		WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		if (log_coop_frame) {
			CoopDebugLog::Log("MainLoop WWAudio On_Frame_Update done");
		}
	}
}
	// Give the sound manager a chance to think
  // PROFILE(	"Audio", WWAudioClass::Get_Instance ()->On_Frame_Update (0) );

	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop Windows_Message_Handler start");
	}
   Windows_Message_Handler();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop Windows_Message_Handler done");
	}
#ifdef WWDEBUG
   // Sometimes it is useful to be able to artificially lower the frame rate
   Sleep(cDevOptions::DesiredFrameSleepMs.Get());
#endif

#if 0
{	WWPROFILE( "Random" );
	// spin the Random Generator, a little
	int count = FreeRandom.Get_Int( 5 );
	while ( count-- > 0 ) {
		FreeRandom.Get_Int();
	}
}
#endif

	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop DebugManager::Update start");
	}
   DebugManager::Update();
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop DebugManager::Update done");
	}


	/*
	** Sleep for a while if we are hogging the CPU.
	*/
	if (cNetwork::I_Am_Only_Server()) {
		unsigned int time2 = TIMEGETTIME();
		if (time2 >= time1) {

			/*
			** 16 (approx) for 60 fps. (1000/60)
			*/
			unsigned int diff = time2 - time1;
			if (diff < 16) {
				unsigned int sleep_time = 16 - (time2 - time1);
				Sleep(sleep_time);
			}
		}
	}
	if (log_coop_frame) {
		CoopDebugLog::Log("MainLoop frame done");
	}
}

/*
** MAIN GAME LOOP
*/
int Game_Main_Loop(void)
{
	const unsigned int servicetime = 1000; // Time in milliseconds.

	unsigned int time;
	CoopDebugLog::Log("Game_Main_Loop start");

	// Only run main loop if the init is succesful!
	bool init_ok = Game_Init();
	CoopDebugLog::Log("Game_Main_Loop Game_Init returned %d", init_ok);
	if (init_ok) {
		while ( RunMainLoop ) {
			_Game_Main_Loop_Loop();
		}
		CoopDebugLog::Log("Game_Main_Loop loop exited exitCode=%d", ExitCode);

		// IML: Allow a short period to process any outstanding sound effects before shutdown.
		CoopDebugLog::Log("Game_Main_Loop shutdown audio drain start");
		time = TIMEGETTIME();
		while (TIMEGETTIME() - time < servicetime) {
			WWAudioClass::Get_Instance ()->On_Frame_Update (0);
		}
		CoopDebugLog::Log("Game_Main_Loop shutdown audio drain done");

		CoopDebugLog::Log("Game_Main_Loop Game_Shutdown start");
		Game_Shutdown();
		CoopDebugLog::Log("Game_Main_Loop Game_Shutdown done");
	}

	CoopDebugLog::Log("Game_Main_Loop returning exitCode=%d", ExitCode);
	return ExitCode;
}
