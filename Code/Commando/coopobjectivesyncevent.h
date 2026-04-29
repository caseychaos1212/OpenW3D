/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPOBJECTIVESYNCEVENT_H__
#define __COOPOBJECTIVESYNCEVENT_H__

#include "netclassids.h"
#include "netevent.h"
#include "objectives.h"
#include "vector3.h"
#include "wwstring.h"

class cCoopObjectiveSyncEvent : public cNetEvent
{
public:
	cCoopObjectiveSyncEvent(void);

	void Init_Snapshot(int client_id);
	void Init_Update(int operation, const Objective *objective);
	void Init_Remove(int objective_id);

	static void Send_Snapshot(int client_id);

	virtual void Export_Creation(BitStreamClass &packet) override;
	virtual void Import_Creation(BitStreamClass &packet) override;
	virtual uint32 Get_Network_Class_ID(void) const override { return NETCLASSID_COOPOBJECTIVESYNCEVENT; }

private:
	struct ObjectiveState
	{
		int ID;
		int Type;
		int Status;
		int LongDescriptionID;
		int ShortDescriptionID;
		StringClass DescriptionSoundFilename;
		StringClass HUDPogTextureName;
		int HUDMessageStringID;
		float HUDPriority;
		bool DrawBlip;
		Vector3 Position;
		float BlipIntensity;
		int ObjectID;
	};

	virtual void Act(void) override;

	static void Copy_Objective(ObjectiveState &state, const Objective *objective);
	static void Export_Objective(BitStreamClass &packet, const ObjectiveState &state);
	static void Import_Objective(BitStreamClass &packet, ObjectiveState &state);
	static void Apply_Objective(const ObjectiveState &state);

	int Operation;
	int ObjectiveID;
	ObjectiveState State;
};

#endif
