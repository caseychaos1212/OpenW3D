/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPLEVELTRANSITIONEVENT_H__
#define __COOPLEVELTRANSITIONEVENT_H__

#include "gamedata.h"
#include "netclassids.h"
#include "netevent.h"

//-----------------------------------------------------------------------------
//
// A S->C event that tells connected co-op clients to load the next campaign map.
//
class	cCoopLevelTransitionEvent : public cNetEvent
{
public:
	cCoopLevelTransitionEvent(void);

	void						Init(const char *map_name, int difficulty_level);

	virtual void			Export_Creation(BitStreamClass &packet) override;
	virtual void			Import_Creation(BitStreamClass &packet) override;
	virtual uint32			Get_Network_Class_ID(void) const override				{return NETCLASSID_COOPLEVELTRANSITIONEVENT;}

private:

	virtual void			Act(void) override;

	char						MapName[MAX_MAPNAME_SIZE];
	int						DifficultyLevel;
};

//-----------------------------------------------------------------------------

#endif	// __COOPLEVELTRANSITIONEVENT_H__
