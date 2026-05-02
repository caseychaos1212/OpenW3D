/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPRESPAWNSTATE_H__
#define __COOPRESPAWNSTATE_H__

#include "widestring.h"

class cCoopRespawnState
{
public:
	static void Set_Waiting(bool waiting, int spectate_object_id);
	static bool Is_Waiting(void);
	static int Get_Spectate_Object_Id(void);
	static void Update_Spectate_Camera(void);
	static const WideStringClass &Get_Waiting_Text(void);
};

#endif // __COOPRESPAWNSTATE_H__
