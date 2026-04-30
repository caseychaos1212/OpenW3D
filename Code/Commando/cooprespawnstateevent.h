/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPRESPAWNSTATEEVENT_H__
#define __COOPRESPAWNSTATEEVENT_H__

#include "netclassids.h"
#include "netevent.h"

class cCoopRespawnStateEvent : public cNetEvent
{
public:
	cCoopRespawnStateEvent(void);

	void Init(int client_id, bool waiting, int spectate_object_id);
	void Act(void);

	virtual uint32 Get_Network_Class_ID(void) const override { return NETCLASSID_COOPRESPAWNSTATEEVENT; }
	virtual void Import_Creation(BitStreamClass &packet) override;
	virtual void Export_Creation(BitStreamClass &packet) override;

private:
	bool Waiting;
	int SpectateObjectId;
};

#endif // __COOPRESPAWNSTATEEVENT_H__
