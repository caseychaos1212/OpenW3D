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
 *                     $Archive:: /Commando/Code/commando/dlgmplangamelist.cpp     $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 3/28/02 4:31p                                               $*
 *                                                                                             *
 *                    $Revision:: 41                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "dlgmplangamelist.h"
#include "win.h"
#include "listctrl.h"
#include "gamedata.h"
#include "gamechannel.h"
#include "gamechanlist.h"
#include "gameinitmgr.h"
#include "wolgmode.h"
#include "dialogmgr.h"
#include "cnetwork.h"
#include "ww3d.h"
#include "renegadedialog.h"
#include "renegadedialogmgr.h"
#include "translatedb.h"
#include "string_ids.h"
#include "dialogcontrol.h"
#include "specialbuilds.h"
#include "editctrl.h"
#include "DlgPasswordPrompt.h"
#include "DlgMessageBox.h"
#include "registry.h"
#include "_globals.h"
#include "dlgmplanhostoptions.h"
#include "gdcoopmission.h"
#include "gamespyadmin.h"

#include <stdlib.h>
#include <string.h>

bool MPLanGameListMenuClass::UpdateNickname = false;
bool MPLanGameListMenuClass::IsCoopList = false;
static RectClass DefaultGameListRect;
static bool HasDefaultGameListRect = false;

////////////////////////////////////////////////////////////////
//	Local constants
////////////////////////////////////////////////////////////////
enum
{
	COL_ICON				= 0,
	COL_HOST_NAME,
	COL_GAME_NAME,
	COL_GAME_MAP,
	COL_PLAYERS,
	COL_MAX
};


////////////////////////////////////////////////////////////////
//	Static member initialization
////////////////////////////////////////////////////////////////
MPLanGameListMenuClass *	MPLanGameListMenuClass::_TheInstance	= NULL;


////////////////////////////////////////////////////////////////
//
//	MPLanGameListMenuClass
//
////////////////////////////////////////////////////////////////
MPLanGameListMenuClass::MPLanGameListMenuClass (void)	:
	UpdateTimer (0),
	MenuDialogClass (GetRenegadeDialog(RenegadeDialogID::IDD_MP_LAN_GAME_LIST))
{
	WWDEBUG_SAY(("MPLanGameListMenu instantiated\n"));
}


MPLanGameListMenuClass::~MPLanGameListMenuClass()
{
	WWDEBUG_SAY(("MPLanGameListMenu destroyed\n"));
}

////////////////////////////////////////////////////////////////
//
//	On_Init_Dialog
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_Init_Dialog (void)
{
/*
#ifdef BETACLIENT
	Get_Dlg_Item(IDC_MENU_MP_LAN_HOST_BUTTON)->Enable(false);
#endif // BETACLIENT
*/

#ifdef FREEDEDICATEDSERVER
	Get_Dlg_Item(IDC_JOIN_GAME_BUTTON)->Enable(false);
#endif // FREEDEDICATEDSERVER

	//
	//	Get a pointer to the list control
	//
	ListCtrlClass *list_ctrl = (ListCtrlClass *)Get_Dlg_Item (IDC_GAME_LIST_CTRL);
	if (list_ctrl != NULL) {
		DefaultGameListRect = list_ctrl->Get_Window_Rect();
		HasDefaultGameListRect = true;

		WideStringClass col_name;

		//
		//	Configure the columns
		//
		col_name = TRANSLATE (IDS_MP_GAME_LIST_HEADER_ICON);
		list_ctrl->Add_Column (col_name, 0.1F, Vector3 (1, 1, 1));

		col_name = TRANSLATE(IDS_MP_GAME_LIST_HEADER_HOST_NAME);
		list_ctrl->Add_Column (col_name, 0.175F, Vector3 (1, 1, 1));

		col_name = TRANSLATE (IDS_MP_GAME_LIST_HEADER_GAME_NAME);
		list_ctrl->Add_Column (col_name, 0.325F, Vector3 (1, 1, 1));

		col_name = TRANSLATE (IDS_MP_GAME_LIST_HEADER_GAME_MAP);
		list_ctrl->Add_Column (col_name, 0.2F, Vector3 (1, 1, 1));

		col_name = TRANSLATE (IDS_MP_GAME_LIST_HEADER_PLAYERS);
		list_ctrl->Add_Column (col_name, 0.1F, Vector3 (1, 1, 1));

		col_name = TRANSLATE (IDS_MP_GAME_LIST_HEADER_PING);
		list_ctrl->Add_Column (col_name, 0.1F, Vector3 (1, 1, 1));

		//
		//	Refresh the game list in one second
		//
		UpdateTimer = 500;
	}

	//
	//	Put the nickname into the nickname edit control
	//
	EditCtrlClass* nameEdit = (EditCtrlClass*)Get_Dlg_Item(IDC_NICKNAME_EDIT);
	assert(nameEdit != NULL);

	if (nameEdit) {
		//nameEdit->Set_Text_Limit(32);
		nameEdit->Set_Text_Limit(9);
		nameEdit->Set_Text(cNetInterface::Get_Nickname());

		bool enable = (nameEdit->Get_Text_Length() > 0);
		Enable_Dlg_Item(IDC_JOIN_GAME_BUTTON, enable);
		Enable_Dlg_Item(IDC_MENU_MP_LAN_HOST_BUTTON, enable);
	}

	EditCtrlClass *address_edit = (EditCtrlClass *)Get_Dlg_Item(IDC_COOP_DIRECT_ADDRESS_EDIT);
	if (address_edit != NULL) {
		address_edit->Set_Text_Limit(63);
	}

	EditCtrlClass *port_edit = (EditCtrlClass *)Get_Dlg_Item(IDC_COOP_DIRECT_PORT_EDIT);
	if (port_edit != NULL) {
		port_edit->Set_Text_Limit(5);
		port_edit->Set_Text(U_CHAR("4848"));
	}

	Apply_Coop_List_Layout();

	cGameChannelList::Remove_All();
	MenuDialogClass::On_Init_Dialog ();
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Destroy
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_Destroy (void)
{
	//
	//	Set the new nickname
	//
	WideStringClass nickname = Get_Dlg_Item_Text (IDC_NICKNAME_EDIT);
	cNetInterface::Set_Nickname (nickname);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Command
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_Command (int ctrl_id, int message_id, unsigned int param)
{
	switch (ctrl_id) {

		case IDC_REFRESH_GAME_LIST:
			Update_Game_List ();
			break;

		case IDC_JOIN_GAME_BUTTON:
			Join_Game ();
			break;

		case IDC_COOP_DIRECT_CONNECT_BUTTON:
			Start_Direct_Coop_Connect();
			break;

		case IDC_MENU_MP_LAN_HOST_BUTTON:
		{
			//
			//	Set the new nickname
			//
			WideStringClass nickname = Get_Dlg_Item_Text (IDC_NICKNAME_EDIT);
			cNetInterface::Set_Nickname (nickname);

			/*
			//
			//	Cache this value, don't know if we need to or not anymore...
			//
			RegistryClass registry (APPLICATION_SUB_KEY_NAME_WOLSETTINGS);
			if (registry.Is_Valid ()) {
				registry.Set_Int (REG_VALUE_LAST_GAME_TYPE, 0);
			}
			*/

			//
			//	Create the new game data
			//
			if ( PTheGameData != NULL ) {
				delete PTheGameData;
				PTheGameData = NULL;
			}
			cGameData::GameTypeEnum game_type = IsCoopList ? cGameData::GAME_TYPE_COOP_MISSION : cGameData::GAME_TYPE_CNC;
			if (IsCoopList && !GameInitMgrClass::Is_Coop_LAN_Initialized()) {
				GameInitMgrClass::Initialize_Coop_LAN();
			}
			PTheGameData = cGameData::Create_Game_Of_Type (game_type);
			WWASSERT(PTheGameData != NULL);

			// LAN games are NEVER quickmatch
			The_Game()->Set_QuickMatch_Server(false);

			The_Game()->Load_From_Server_Config ();

			//
			//	Go to the host game options menu
			//
			START_DIALOG (MPLanHostOptionsMenuClass);
			break;
		}
	}


	MenuDialogClass::On_Command (ctrl_id, message_id, param);
	return ;
}


////////////////////////////////////////////////////////////////
//
//	Apply_Coop_List_Layout
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Apply_Coop_List_Layout(void)
{
	ListCtrlClass *list_ctrl = (ListCtrlClass *)Get_Dlg_Item(IDC_GAME_LIST_CTRL);
	if (list_ctrl != NULL) {
		if (!HasDefaultGameListRect) {
			DefaultGameListRect = list_ctrl->Get_Window_Rect();
			HasDefaultGameListRect = true;
		}

		RectClass list_rect = DefaultGameListRect;
		if (IsCoopList) {
			list_rect.Bottom = list_rect.Top + (DefaultGameListRect.Height() * (142.0F / 156.0F));
		}
		list_ctrl->Set_Window_Rect(list_rect);
	}

	Set_Dlg_Item_Text(IDC_GAME_LIST_TITLE, IsCoopList ? U_CHAR("Co-op Campaign") : TRANSLATE(IDS_MENU_TEXT280));
	Get_Dlg_Item(IDC_COOP_DIRECT_ADDRESS_STATIC)->Show(IsCoopList);
	Get_Dlg_Item(IDC_COOP_DIRECT_ADDRESS_EDIT)->Show(IsCoopList);
	Get_Dlg_Item(IDC_COOP_DIRECT_PORT_STATIC)->Show(IsCoopList);
	Get_Dlg_Item(IDC_COOP_DIRECT_PORT_EDIT)->Show(IsCoopList);
	Get_Dlg_Item(IDC_COOP_DIRECT_CONNECT_BUTTON)->Show(IsCoopList);
	Update_Coop_Direct_Connect_Enable();
}


////////////////////////////////////////////////////////////////
//
//	Start_Direct_Coop_Connect
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Start_Direct_Coop_Connect(void)
{
	if (!IsCoopList) {
		return;
	}

	WideStringClass nickname = Get_Dlg_Item_Text(IDC_NICKNAME_EDIT);
	nickname.Trim();
	if (nickname.Is_Empty()) {
		DlgMsgBox::DoDialog(U_CHAR("Co-op Campaign"), U_CHAR("Enter a player name before connecting."));
		return;
	}
	cNetInterface::Set_Nickname(nickname);

	StringClass address;
	WideStringClass wide_address = Get_Dlg_Item_Text(IDC_COOP_DIRECT_ADDRESS_EDIT);
	wide_address.Trim();
	wide_address.Convert_To(address);
	address.Trim();

	StringClass port_text;
	WideStringClass wide_port = Get_Dlg_Item_Text(IDC_COOP_DIRECT_PORT_EDIT);
	wide_port.Trim();
	wide_port.Convert_To(port_text);
	port_text.Trim();

	if (address.Is_Empty()) {
		DlgMsgBox::DoDialog(U_CHAR("Co-op Campaign"), U_CHAR("Enter an IP address before connecting."));
		return;
	}

	USHORT port = 4848;
	if (!port_text.Is_Empty()) {
		char *end_port = NULL;
		long parsed_port = ::strtol(port_text.Peek_Buffer(), &end_port, 10);
		if (end_port == NULL || *end_port != 0 || parsed_port <= 0 || parsed_port > 65535) {
			DlgMsgBox::DoDialog(U_CHAR("Co-op Campaign"), U_CHAR("Enter a valid port between 1 and 65535."));
			return;
		}
		port = (USHORT)parsed_port;
	}

	char address_buffer[128] = { 0 };
	::strncpy(address_buffer, address.Peek_Buffer(), sizeof(address_buffer) - 1);
	char *address_port = ::strchr(address_buffer, ':');
	if (address_port != NULL) {
		*address_port = 0;
		char *end_port = NULL;
		long parsed_port = ::strtol(address_port + 1, &end_port, 10);
		if (end_port == NULL || *end_port != 0 || parsed_port <= 0 || parsed_port > 65535) {
			DlgMsgBox::DoDialog(U_CHAR("Co-op Campaign"), U_CHAR("Enter a valid port between 1 and 65535."));
			return;
		}
		port = (USHORT)parsed_port;
	}

	ULONG ip = ::inet_addr(address_buffer);
	if (ip == 0 || ip == (ULONG)-1) {
		DlgMsgBox::DoDialog(U_CHAR("Co-op Campaign"), U_CHAR("Enter a valid IPv4 address."));
		return;
	}

	cGameSpyAdmin::Start_Coop_Direct_Connect(ip, port);
	End_Dialog();
}


////////////////////////////////////////////////////////////////
//
//	Update_Coop_Direct_Connect_Enable
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Update_Coop_Direct_Connect_Enable(void)
{
	if (!IsCoopList) {
		Enable_Dlg_Item(IDC_COOP_DIRECT_CONNECT_BUTTON, false);
		return;
	}

	bool has_name = false;
	EditCtrlClass *name_edit = (EditCtrlClass *)Get_Dlg_Item(IDC_NICKNAME_EDIT);
	if (name_edit != NULL) {
		has_name = (name_edit->Get_Text_Length() > 0);
	}

	bool has_address = false;
	EditCtrlClass *address_edit = (EditCtrlClass *)Get_Dlg_Item(IDC_COOP_DIRECT_ADDRESS_EDIT);
	if (address_edit != NULL) {
		has_address = (address_edit->Get_Text_Length() > 0);
	}

	Enable_Dlg_Item(IDC_COOP_DIRECT_CONNECT_BUTTON, has_name && has_address);
}


////////////////////////////////////////////////////////////////
//
//	On_Frame_Update
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_Frame_Update (void)
{
	//
	//	Is it time to update the list?
	//
	UpdateTimer -= WW3D::Get_Frame_Time ();
	if (UpdateTimer <= 0) {

		//
		//	Update the list and reset the timer
		//
		Update_Game_List ();

		if (GameInitMgrClass::Is_WOL_Initialized ()) {
			UpdateTimer = 20000;
		} else {
			UpdateTimer = 1000;
		}
	}

	if (UpdateNickname) {
		EditCtrlClass * nameEdit = (EditCtrlClass*)Get_Dlg_Item(IDC_NICKNAME_EDIT);
		WWASSERT(nameEdit != NULL);
		nameEdit->Set_Text(cNetInterface::Get_Nickname());
		UpdateNickname = false;
	}

	MenuDialogClass::On_Frame_Update ();
	return ;
}


bool MPLanGameListMenuClass::On_Key_Down(uint32 key_id, uint32 key_data)
{
	if (VK_F5 == key_id) {
		Update_Game_List();
		return true;
	}

	return MenuDialogClass::On_Key_Down(key_id, key_data);
}


////////////////////////////////////////////////////////////////
//
//	Update_Game_List
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Update_Game_List (void)
{
	ListCtrlClass *list_ctrl = (ListCtrlClass *)Get_Dlg_Item (IDC_GAME_LIST_CTRL);
	if (list_ctrl == NULL) {
		return ;
	}

	//
	//	Remember the owner of the selected entry so we can select it again as necessary
	//
	WideStringClass selected_owner_name;
	int curr_sel = list_ctrl->Get_Curr_Sel ();
	if (curr_sel != -1) {
		cGameChannel *channel = (cGameChannel *)list_ctrl->Get_Entry_Data (curr_sel, 0);
		if (channel != NULL) {
			selected_owner_name = channel->Get_Game_Data ()->Get_Owner ();
		}
	}

	//
	//	Start fresh
	//
	list_ctrl->Delete_All_Entries ();

	//
	//	Build the game list
	//
	int index = 0;
	bool found_selected = false;
	SLNode<cGameChannel> *objnode = NULL;
	for (objnode = cGameChannelList::Get_Chan_List ()->Head(); objnode; objnode = objnode->Next ()) {

		//
		//	Get a pointer to the channel for this game
		//
		cGameChannel *channel = objnode->Data ();
		WWASSERT (channel != NULL);

		bool is_coop_game = channel->Get_Game_Data()->Is_Coop_Mission();
		if (is_coop_game != IsCoopList) {
			continue;
		}

		//
		//	Insert the entry
		//
		int item_index = list_ctrl->Insert_Entry (index ++, U_CHAR(""));
		if (item_index >= 0) {

			//
			//	Get the owner and game name
			//
			WideStringClass wide_owner_name;
			WideStringClass wide_game_name;
			WideStringClass wide_map_name;

			wide_owner_name = channel->Get_Game_Data ()->Get_Owner();
			//wide_game_name = channel->Get_Game_Data ()->Get_Game_Name ();
			wide_game_name = channel->Get_Game_Data()->Get_Game_Title ();
			wide_map_name.Convert_From (channel->Get_Game_Data ()->Get_Map_Name ());

			//
			//	Get the current and max players for this channel
			//
			int player_count		= channel->Get_Game_Data()->Get_Current_Players ();
			int player_count_max = channel->Get_Game_Data()->Get_Max_Players ();

			//
			//	Build a string that represents the player count
			//
			WideStringClass players_string;
			players_string.Format (U_CHAR("%d/%d"), player_count, player_count_max);

			//
			//	Build a string that represents the connection (ping) speed
			//
			int ping = 0;

			WideStringClass ping_string;
			ping_string.Format (U_CHAR("%d"), ping);

			//
			//	If this is a mod'd game, then display the mod_name\map_name...
			//
			if (channel->Get_Game_Data()->Get_Mod_Name ().Get_Length () > 0) {

				//
				//	Strip off the extension for both the map and the mod package
				//
				char map_name[_MAX_FNAME] = { 0 };
				char mod_name[_MAX_FNAME] = { 0 };
				::_splitpath (channel->Get_Game_Data ()->Get_Map_Name (), NULL, NULL, map_name, NULL);
				::_splitpath (channel->Get_Game_Data ()->Get_Mod_Name (), NULL, NULL, mod_name, NULL);

				//
				//	Create the map name from the aggregate of the mod and map
				//
				StringClass ascii_map_name;
				ascii_map_name.Format ("%s/%s", mod_name, map_name);
				wide_map_name.Convert_From (ascii_map_name);
			}

			//
			//	Update the pla
			//
			list_ctrl->Set_Entry_Text (item_index, COL_ICON,		U_CHAR(""));
			list_ctrl->Set_Entry_Text (item_index, COL_HOST_NAME,	wide_owner_name);
			list_ctrl->Set_Entry_Text (item_index, COL_GAME_NAME,	wide_game_name);
			list_ctrl->Set_Entry_Text (item_index, COL_GAME_MAP,	wide_map_name);
			list_ctrl->Set_Entry_Text (item_index, COL_PLAYERS,	players_string);

			//
			//	Associate the channel data inside with the entry
			//
			list_ctrl->Set_Entry_Data (item_index, 0, (uintptr_t)channel);
			channel->Add_Ref ();

			//
			//	Should we select this entry?
			//
			if (wide_owner_name.Compare_No_Case (selected_owner_name) == 0) {
				list_ctrl->Set_Curr_Sel (item_index);
				found_selected = true;
			}
		}

      //
		//	Do we have a mismatched exe or missing map?
		//
		if (channel->Get_Game_Data()->Get_Version_Number() != cNetwork::Get_Exe_Key() ||
			 channel->Get_Game_Data()->Get_Map_Name().Is_Empty()) {

			WideStringClass error_string;
			if (channel->Get_Game_Data()->Do_Exe_Versions_Match() == false) {
				error_string = TRANSLATE (IDS_MENU_EXE_MISMATCH);
			} else if (channel->Get_Game_Data()->Do_String_Versions_Match() == false) {
				error_string = TRANSLATE (IDS_MENU_STRINGS_MISMATCH);
			} else if (channel->Get_Game_Data()->Get_Map_Name().Is_Empty()) {
				error_string = TRANSLATE (IDS_MENU_MISSING_MAP);
			}

			//
			//	Notify the user that they can't join this game
			//
			if (item_index >= 0) {
				//list_ctrl->Set_Entry_Text (item_index, COL_HOST_NAME, error_string);
				list_ctrl->Set_Entry_Text (item_index, COL_GAME_NAME, error_string);
				list_ctrl->Set_Entry_Color (item_index, COL_ICON,			Vector3 (0.5F, 0.5F, 0.5F));
				list_ctrl->Set_Entry_Color (item_index, COL_HOST_NAME,	Vector3 (0.5F, 0.5F, 0.5F));
				list_ctrl->Set_Entry_Color (item_index, COL_GAME_NAME,	Vector3 (0.5F, 0.5F, 0.5F));
				list_ctrl->Set_Entry_Color (item_index, COL_GAME_MAP,		Vector3 (0.5F, 0.5F, 0.5F));
				list_ctrl->Set_Entry_Color (item_index, COL_PLAYERS,		Vector3 (0.5F, 0.5F, 0.5F));
				list_ctrl->Set_Entry_Data (item_index, 1, 1);
			}
		}
	}

	//
	//	Select the first entry by default
	//
	if (found_selected == false && list_ctrl->Get_Entry_Count () > 0) {
		list_ctrl->Set_Curr_Sel (0);
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Join_Game
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Join_Game (void)
{
	ListCtrlClass *list_ctrl = (ListCtrlClass *)Get_Dlg_Item (IDC_GAME_LIST_CTRL);

	//
	//	Get the currently selected index
	//
	int index = list_ctrl->Get_Curr_Sel ();
	if (index >= 0) {

		//
		//	Get the channel data from this entry
		//
		cGameChannel *channel = (cGameChannel *)list_ctrl->Get_Entry_Data (index, 0);
		if (channel != NULL && list_ctrl->Get_Entry_Data (index, 1) == 0) {

			//
			//	Free the old game data (if necessary)
			//
			if (PTheGameData != NULL) {
				delete PTheGameData;
				PTheGameData = NULL;
			}

			//
			//	Create a new game data object that matches the one on the server
			//
			PTheGameData = cGameData::Create_Game_Of_Type (channel->Get_Game_Data ()->Get_Game_Type ());
			WWASSERT(PTheGameData != NULL);
			*PTheGameData = *channel->Get_Game_Data ();

			// If the game to join is passworded then it is necessary for the user to
			// supply the correct password. Therefore prompt the user to enter the
			// password. We will attempt to connect to the game when we receive a
			// signal from the dialog that the user has entered a password.
			if (PTheGameData->IsPassworded.Is_True()) {
				DlgPasswordPrompt::DoDialog(this);
			} else {
				Connect_To_Server();
			}
		}
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
// Handle receipt of password entered signal from the password
// prompt dialog.
//
////////////////////////////////////////////////////////////////
void MPLanGameListMenuClass::ReceiveSignal(DlgPasswordPrompt& passwordDialog)
{
	WWASSERT(PTheGameData != NULL);
	PTheGameData->Set_Password(passwordDialog.GetPassword());
	Connect_To_Server();
}


////////////////////////////////////////////////////////////////
//
//	Connect_To_Server
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Connect_To_Server (void)
{
	//
	//	Set the new nickname
	//
	WideStringClass nickname = Get_Dlg_Item_Text (IDC_NICKNAME_EDIT);
	cNetInterface::Set_Nickname (nickname);

	//
	//	Start the client
	//
	cNetwork::Init_Client ();

	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_ListCtrl_Delete_Entry
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_ListCtrl_Delete_Entry
(
	ListCtrlClass *list_ctrl,
	int				/* ctrl_id */,
	int				item_index
)
{
	//
	//	Get the channel data from this entry
	//
	cGameChannel *channel = (cGameChannel *)list_ctrl->Get_Entry_Data (item_index, 0);
	list_ctrl->Set_Entry_Data (item_index, 0, 0);

	if (channel != NULL) {
		channel->Release_Ref ();
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_ListCtrl_DblClk
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_ListCtrl_DblClk
(
	ListCtrlClass * /* list_ctrl */,
	int				/* ctrl_id */,
	int				/* item_index */
)
{
	Join_Game ();
	return ;
}


void MPLanGameListMenuClass::On_EditCtrl_Change(EditCtrlClass* edit, int id)
{
	if (IDC_NICKNAME_EDIT == id) {
		// Do not allow leading or trailing whitespace
		WideStringClass text(0, true);
		text = edit->Get_Text();
		text.Trim();
		edit->Set_Text(text);

		bool enable = (edit->Get_Text_Length() > 0);
		Enable_Dlg_Item(IDC_JOIN_GAME_BUTTON, enable);
		Enable_Dlg_Item(IDC_MENU_MP_LAN_HOST_BUTTON, enable);
		Update_Coop_Direct_Connect_Enable();
	} else if (IDC_COOP_DIRECT_ADDRESS_EDIT == id || IDC_COOP_DIRECT_PORT_EDIT == id) {
		Update_Coop_Direct_Connect_Enable();
	}
}


////////////////////////////////////////////////////////////////
//
//	Display
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Display (void)
{
	IsCoopList = false;

	//
	//	Create the dialog if necessary, otherwise simply bring it to the front
	//
	if (_TheInstance == NULL) {
		START_DIALOG (MPLanGameListMenuClass);
	} else {
		_TheInstance->Apply_Coop_List_Layout();
		if (_TheInstance->Is_Active_Menu () == false) {
			DialogMgrClass::Rollback (_TheInstance);
		}
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	Display_Coop
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::Display_Coop (void)
{
	IsCoopList = true;

	//
	//	Create the dialog if necessary, otherwise simply bring it to the front
	//
	if (_TheInstance == NULL) {
		START_DIALOG (MPLanGameListMenuClass);
	} else {
		_TheInstance->Apply_Coop_List_Layout();
		if (_TheInstance->Is_Active_Menu () == false) {
			DialogMgrClass::Rollback (_TheInstance);
		}
	}

	return ;
}


////////////////////////////////////////////////////////////////
//
//	On_Last_Menu_Ending
//
////////////////////////////////////////////////////////////////
void
MPLanGameListMenuClass::On_Last_Menu_Ending (void)
{
	RenegadeDialogMgrClass::Goto_Location (RenegadeDialogMgrClass::LOC_MAIN_MENU);
	return ;
}





















	/*
	if (UpdateNickname && !g_awaiting_edit) {
		::MessageBeep(MB_OK);//XXX
		EditCtrlClass* nameEdit = (EditCtrlClass*)Get_Dlg_Item(IDC_NICKNAME_EDIT);
		assert(nameEdit != NULL);
		nameEdit->Set_Focus();
		g_awaiting_edit = true;
	}
	*/

		//g_awaiting_edit = false;
		//UpdateNickname = false;
//bool g_awaiting_edit = false;//XXX
//#include "dlgmplangametype.h"
