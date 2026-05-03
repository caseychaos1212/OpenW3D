/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __DLGCOOPLOBBY_H__
#define __DLGCOOPLOBBY_H__

#include "menudialog.h"

class EditCtrlClass;
class ListCtrlClass;

class CoopLobbyDialogClass : public MenuDialogClass
{
public:
	CoopLobbyDialogClass(void);

	void On_Init_Dialog(void) override;
	void On_Periodic(void) override;
	void On_Command(int ctrl_id, int message_id, unsigned int param) override;
	void On_EditCtrl_Enter_Pressed(EditCtrlClass *edit_ctrl, int ctrl_id) override;

private:
	void Refresh(void);
	void Refresh_Header(void);
	void Refresh_Player_List(void);
	void Refresh_Options_List(void);
	void Refresh_Results_List(void);
	void Refresh_Stats_List(void);
	void Refresh_Chat_List(void);
	void Send_Chat(void);
	void Add_Single_Column(ListCtrlClass *list_ctrl, const unichar_t *title);
};

#endif // __DLGCOOPLOBBY_H__
