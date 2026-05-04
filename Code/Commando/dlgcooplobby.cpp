/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "dlgcooplobby.h"

#include "cooplobbymgr.h"
#include "cnetwork.h"
#include "dialogcontrol.h"
#include "editctrl.h"
#include "listctrl.h"
#include "renegadedialog.h"

static void Format_Time(float seconds, WideStringClass &text)
{
	int total_seconds = (int)seconds;
	int hours = total_seconds / 3600;
	int minutes = (total_seconds / 60) % 60;
	int secs = total_seconds % 60;

	if (hours > 0) {
		text.Format(U_CHAR("%d:%02d:%02d"), hours, minutes, secs);
	} else {
		text.Format(U_CHAR("%02d:%02d"), minutes, secs);
	}
}

static const unichar_t *Yes_No(bool value)
{
	return value ? U_CHAR("Yes") : U_CHAR("No");
}

CoopLobbyDialogClass::CoopLobbyDialogClass(void) :
	MenuDialogClass(GetRenegadeDialog(RenegadeDialogID::IDD_COOP_LOBBY))
{
}

void CoopLobbyDialogClass::On_Init_Dialog(void)
{
	ListCtrlClass *players = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_PLAYER_LIST);
	if (players != NULL) {
		players->Add_Column(U_CHAR("Player"), 0.36f, Vector3(1, 1, 1));
		players->Add_Column(U_CHAR("Ready"), 0.18f, Vector3(1, 1, 1));
		players->Add_Column(U_CHAR("Score"), 0.18f, Vector3(1, 1, 1));
		players->Add_Column(U_CHAR("Deaths"), 0.16f, Vector3(1, 1, 1));
		players->Add_Column(U_CHAR("Ping"), 0.12f, Vector3(1, 1, 1));
	}

	Add_Single_Column((ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_OPTIONS_LIST), U_CHAR("Host Options"));
	Add_Single_Column((ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_RESULTS_LIST), U_CHAR("Mission Results"));

	ListCtrlClass *stats = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_STATS_LIST);
	if (stats != NULL) {
		stats->Add_Column(U_CHAR("Player"), 0.30f, Vector3(1, 1, 1));
		stats->Add_Column(U_CHAR("Kills"), 0.12f, Vector3(1, 1, 1));
		stats->Add_Column(U_CHAR("Acc"), 0.12f, Vector3(1, 1, 1));
		stats->Add_Column(U_CHAR("Pups"), 0.12f, Vector3(1, 1, 1));
		stats->Add_Column(U_CHAR("Veh"), 0.12f, Vector3(1, 1, 1));
		stats->Add_Column(U_CHAR("Bld"), 0.12f, Vector3(1, 1, 1));
		stats->Add_Column(U_CHAR("Cred"), 0.10f, Vector3(1, 1, 1));
	}

	Add_Single_Column((ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_CHAT_LIST), U_CHAR("Chat"));

	EditCtrlClass *edit = (EditCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_CHAT_EDIT);
	if (edit != NULL) {
		edit->Set_Text_Limit(100);
		edit->Set_Focus();
	}

	Refresh();
	MenuDialogClass::On_Init_Dialog();
}

void CoopLobbyDialogClass::On_Periodic(void)
{
	MenuDialogClass::On_Periodic();
	Refresh();
}

void CoopLobbyDialogClass::On_Command(int ctrl_id, int message_id, unsigned int param)
{
	switch (ctrl_id) {
		case IDC_COOP_LOBBY_SEND_BUTTON:
			Send_Chat();
			break;

		case IDC_COOP_LOBBY_READY_BUTTON:
			CoopLobbyMgrClass::Toggle_Local_Ready();
			Refresh();
			break;

		case IDC_COOP_LOBBY_START_BUTTON:
			CoopLobbyMgrClass::Start_Pending_Map();
			break;

		case IDCANCEL:
			break;

		default:
			MenuDialogClass::On_Command(ctrl_id, message_id, param);
			break;
	}
}

void CoopLobbyDialogClass::On_EditCtrl_Enter_Pressed(EditCtrlClass *edit_ctrl, int ctrl_id)
{
	if (ctrl_id == IDC_COOP_LOBBY_CHAT_EDIT) {
		Send_Chat();
	} else {
		MenuDialogClass::On_EditCtrl_Enter_Pressed(edit_ctrl, ctrl_id);
	}
}

void CoopLobbyDialogClass::Refresh(void)
{
	if (!CoopLobbyMgrClass::Is_Active()) {
		End_Dialog();
		return;
	}

	Refresh_Layout();
	Refresh_Header();
	Refresh_Player_List();
	Refresh_Options_List();
	if (CoopLobbyMgrClass::Get_Phase() == CoopLobbyMgrClass::PHASE_BETWEEN_LEVELS) {
		Refresh_Results_List();
		Refresh_Stats_List();
	}
	Refresh_Chat_List();
}

void CoopLobbyDialogClass::Refresh_Layout(void)
{
	bool show_results = CoopLobbyMgrClass::Get_Phase() == CoopLobbyMgrClass::PHASE_BETWEEN_LEVELS;

	if (show_results) {
		Set_Control_Rect(IDC_COOP_LOBBY_PLAYER_LIST, 8, 41, 176, 78);
		Set_Control_Rect(IDC_COOP_LOBBY_OPTIONS_LIST, 190, 41, 202, 78);
		Set_Control_Rect(IDC_COOP_LOBBY_RESULTS_LIST, 8, 126, 176, 70);
		Set_Control_Rect(IDC_COOP_LOBBY_STATS_LIST, 190, 126, 202, 70);
	} else {
		Set_Control_Rect(IDC_COOP_LOBBY_PLAYER_LIST, 8, 41, 176, 155);
		Set_Control_Rect(IDC_COOP_LOBBY_OPTIONS_LIST, 190, 41, 202, 155);
	}

	Show_Control(IDC_COOP_LOBBY_RESULTS_LIST, show_results);
	Show_Control(IDC_COOP_LOBBY_STATS_LIST, show_results);
	Set_Control_Rect(IDC_COOP_LOBBY_CHAT_LIST, 8, 203, 384, 45);
	Set_Control_Rect(IDC_COOP_LOBBY_CHAT_EDIT, 8, 255, 236, 14);
	Set_Control_Rect(IDC_COOP_LOBBY_SEND_BUTTON, 250, 252, 42, 19);
	Set_Control_Rect(IDC_COOP_LOBBY_READY_BUTTON, 298, 252, 45, 19);
	Set_Control_Rect(IDC_COOP_LOBBY_START_BUTTON, 349, 252, 43, 19);
}

void CoopLobbyDialogClass::Refresh_Header(void)
{
	WideStringClass title;
	switch (CoopLobbyMgrClass::Get_Phase()) {
		case CoopLobbyMgrClass::PHASE_PRE_GAME:
			title = U_CHAR("Co-op Campaign Lobby");
			break;
		case CoopLobbyMgrClass::PHASE_BETWEEN_LEVELS:
			title = U_CHAR("Co-op Mission Results");
			break;
		case CoopLobbyMgrClass::PHASE_STARTING_MAP:
			title = U_CHAR("Starting Co-op Mission");
			break;
		default:
			title = U_CHAR("Co-op Lobby");
			break;
	}
	Set_Dlg_Item_Text(IDC_COOP_LOBBY_TITLE, title);

	WideStringClass map_name;
	map_name.Convert_From(CoopLobbyMgrClass::Get_Pending_Map());
	WideStringClass next;
	next.Format(U_CHAR("Next mission: %s"), (const unichar_t *)map_name);
	Set_Dlg_Item_Text(IDC_COOP_LOBBY_NEXT_MAP, next);

	DialogControlClass *start = Get_Dlg_Item(IDC_COOP_LOBBY_START_BUTTON);
	DialogControlClass *ready = Get_Dlg_Item(IDC_COOP_LOBBY_READY_BUTTON);
	if (start != NULL) {
		start->Show(cNetwork::I_Am_Server());
		start->Enable(CoopLobbyMgrClass::Get_Phase() != CoopLobbyMgrClass::PHASE_STARTING_MAP);
	}
	if (ready != NULL) {
		ready->Show(!cNetwork::I_Am_Server());
		WideStringClass ready_text = CoopLobbyMgrClass::Get_Local_Ready() ? U_CHAR("Not Ready") : U_CHAR("Ready");
		Set_Dlg_Item_Text(IDC_COOP_LOBBY_READY_BUTTON, ready_text);
	}
}

void CoopLobbyDialogClass::Refresh_Player_List(void)
{
	ListCtrlClass *list = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_PLAYER_LIST);
	if (list == NULL) {
		return;
	}

	list->Delete_All_Entries();

	const CoopLobbyMgrClass::PlayerStats *players = CoopLobbyMgrClass::Get_Player_Stats();
	int count = CoopLobbyMgrClass::Get_Player_Count();
	for (int index = 0; index < count; index++) {
		int entry = list->Insert_Entry(index, players[index].Name);
		if (entry >= 0) {
			list->Set_Entry_Text(entry, 1, players[index].Ready ? U_CHAR("Yes") : U_CHAR("-"));
			list->Set_Entry_Int(entry, 2, players[index].Score);
			list->Set_Entry_Int(entry, 3, players[index].Deaths);
			list->Set_Entry_Int(entry, 4, players[index].Ping);
		}
	}
}

void CoopLobbyDialogClass::Refresh_Options_List(void)
{
	ListCtrlClass *list = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_OPTIONS_LIST);
	if (list == NULL) {
		return;
	}

	list->Delete_All_Entries();
	const CoopLobbyMgrClass::HostOptions &options = CoopLobbyMgrClass::Get_Host_Options();

	WideStringClass line;
	line.Format(U_CHAR("Difficulty: %d"), options.DifficultyLevel);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Sprint: %s"), Yes_No(options.EnableSprint));
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Pickups disabled: H %s  A %s  Ammo %s"),
		Yes_No(options.DisableHealthPickups),
		Yes_No(options.DisableArmorPickups),
		Yes_No(options.DisableAmmoPickups));
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Enemy health/damage: %.2f / %.2f"), options.EnemyHealthMultiplier, options.EnemyDamageMultiplier);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Death penalty: %d"), options.DeathScorePenalty);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("AI sight/hearing: %.2f / %.2f"), options.AISightMultiplier, options.AIHearingMultiplier);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("AI combat: wander %s  retarget %s  unit types %s"),
		Yes_No(options.AIEnableAttackWander),
		Yes_No(options.AIEnableDamageRetarget),
		Yes_No(options.AIEnableUnitCombatTypes));
	list->Insert_Entry(list->Get_Entry_Count(), line);
}

void CoopLobbyDialogClass::Refresh_Results_List(void)
{
	ListCtrlClass *list = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_RESULTS_LIST);
	if (list == NULL) {
		return;
	}

	list->Delete_All_Entries();
	const CoopLobbyMgrClass::MissionResults &results = CoopLobbyMgrClass::Get_Mission_Results();
	if (!results.HasResults) {
		list->Insert_Entry(0, U_CHAR("No completed mission yet."));
		return;
	}

	WideStringClass map_name;
	map_name.Convert_From(results.MapName);
	WideStringClass line;
	line.Format(U_CHAR("Completed: %s"), (const unichar_t *)map_name);
	list->Insert_Entry(list->Get_Entry_Count(), line);

	WideStringClass time_text;
	Format_Time(results.CompletionTime, time_text);
	line.Format(U_CHAR("Time: %s  Deaths: %d"), (const unichar_t *)time_text, results.TeamDeaths);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Secondary: %d / %d"), results.CompletedSecondaryObjectives, results.SecondaryObjectives);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Bonus: %d / %d"), results.CompletedTertiaryObjectives, results.TertiaryObjectives);
	list->Insert_Entry(list->Get_Entry_Count(), line);
	line.Format(U_CHAR("Stars: overall %d  time %d  difficulty %d  objectives %d  survival %d"),
		results.OverallStars,
		results.TimeStars,
		results.DifficultyStars,
		results.SecondaryStars,
		results.SurvivalStars);
	list->Insert_Entry(list->Get_Entry_Count(), line);
}

void CoopLobbyDialogClass::Refresh_Stats_List(void)
{
	ListCtrlClass *list = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_STATS_LIST);
	if (list == NULL) {
		return;
	}

	list->Delete_All_Entries();
	const CoopLobbyMgrClass::PlayerStats *players = CoopLobbyMgrClass::Get_Player_Stats();
	int count = CoopLobbyMgrClass::Get_Player_Count();
	for (int index = 0; index < count; index++) {
		int entry = list->Insert_Entry(index, players[index].Name);
		if (entry >= 0) {
			WideStringClass text;
			list->Set_Entry_Int(entry, 1, players[index].EnemiesKilled);
			text.Format(U_CHAR("%.0f%%"), players[index].Accuracy);
			list->Set_Entry_Text(entry, 2, text);
			list->Set_Entry_Int(entry, 3, players[index].Powerups);
			list->Set_Entry_Int(entry, 4, players[index].VehiclesDestroyed);
			list->Set_Entry_Int(entry, 5, players[index].BuildingsDestroyed);
			list->Set_Entry_Int(entry, 6, (int)players[index].CreditsGranted);
		}
	}
}

void CoopLobbyDialogClass::Refresh_Chat_List(void)
{
	ListCtrlClass *list = (ListCtrlClass *)Get_Dlg_Item(IDC_COOP_LOBBY_CHAT_LIST);
	if (list == NULL) {
		return;
	}

	list->Delete_All_Entries();
	const WideStringClass *chat = CoopLobbyMgrClass::Get_Chat_Log();
	int count = CoopLobbyMgrClass::Get_Chat_Count();
	for (int index = 0; index < count; index++) {
		list->Insert_Entry(list->Get_Entry_Count(), chat[index]);
	}
}

void CoopLobbyDialogClass::Send_Chat(void)
{
	WideStringClass message(0, true);
	message = Get_Dlg_Item_Text(IDC_COOP_LOBBY_CHAT_EDIT);
	CoopLobbyMgrClass::Send_Chat_Message(message);
	Set_Dlg_Item_Text(IDC_COOP_LOBBY_CHAT_EDIT, U_CHAR(""));
}

void CoopLobbyDialogClass::Add_Single_Column(ListCtrlClass *list_ctrl, const unichar_t *title)
{
	if (list_ctrl != NULL) {
		list_ctrl->Add_Column(title, 1.0f, Vector3(1, 1, 1));
	}
}

void CoopLobbyDialogClass::Set_Control_Rect(int ctrl_id, int x, int y, int width, int height)
{
	DialogControlClass *control = Get_Dlg_Item(ctrl_id);
	if (control == NULL) {
		return;
	}

	const RectClass &dialog_rect = Get_Rect();
	float scale_x = dialog_rect.Width() / 400.0f;
	float scale_y = dialog_rect.Height() / 300.0f;

	RectClass rect;
	rect.Left = dialog_rect.Left + (float)x * scale_x;
	rect.Top = dialog_rect.Top + (float)y * scale_y;
	rect.Right = rect.Left + (float)width * scale_x;
	rect.Bottom = rect.Top + (float)height * scale_y;
	control->Set_Window_Rect(rect);
}

void CoopLobbyDialogClass::Show_Control(int ctrl_id, bool show)
{
	DialogControlClass *control = Get_Dlg_Item(ctrl_id);
	if (control != NULL) {
		control->Show(show);
	}
}
