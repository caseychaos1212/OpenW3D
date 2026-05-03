/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPLOBBYREADYEVENT_H__
#define __COOPLOBBYREADYEVENT_H__

#include "netclassids.h"
#include "netevent.h"

class cCoopLobbyReadyEvent : public cNetEvent
{
public:
	cCoopLobbyReadyEvent(void);

	void Init(bool ready);

	virtual void Export_Creation(BitStreamClass &packet) override;
	virtual void Import_Creation(BitStreamClass &packet) override;
	virtual uint32 Get_Network_Class_ID(void) const override { return NETCLASSID_COOPLOBBYREADYEVENT; }

private:
	virtual void Act(void) override;

	int SenderId;
	bool Ready;
};

#endif // __COOPLOBBYREADYEVENT_H__
