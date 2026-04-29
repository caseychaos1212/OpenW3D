/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPCONTROLLOCKEVENT_H__
#define __COOPCONTROLLOCKEVENT_H__

#include "netclassids.h"
#include "netevent.h"

class cCoopControlLockEvent : public cNetEvent
{
public:
	cCoopControlLockEvent(void);

	void Init(bool enabled);

	virtual void Export_Creation(BitStreamClass &packet) override;
	virtual void Import_Creation(BitStreamClass &packet) override;
	virtual uint32 Get_Network_Class_ID(void) const override { return NETCLASSID_COOPCONTROLLOCKEVENT; }

private:
	virtual void Act(void) override;

	bool Enabled;
};

#endif
