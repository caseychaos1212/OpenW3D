/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#ifndef __COOPCAMERAEVENT_H__
#define __COOPCAMERAEVENT_H__

#include "netclassids.h"
#include "netevent.h"
#include "vector3.h"
#include "wwstring.h"

class ScriptableGameObj;
typedef ScriptableGameObj GameObject;

class cCoopCameraEvent : public cNetEvent
{
public:

	cCoopCameraEvent(void);

	void Init_Camera_Host(GameObject *obj);
	void Init_Force_Look(const Vector3 &target);
	void Act(void);
	static void Sync_Current_Camera_State(int client_id);
    static void Reset(bool new_session = false);
    static void Update();

	virtual unsigned int Get_Network_Class_ID(void) const override { return NETCLASSID_COOPCAMERAEVENT; }
	virtual void Import_Creation(BitStreamClass &packet) override;
	virtual void Export_Creation(BitStreamClass &packet) override;

private:
    void Capture_State();
    StringClass MapName;
    unsigned int Sequence;
    bool HasForceLook;
	int HostObjectId;
	Vector3 Target;
};

#endif // __COOPCAMERAEVENT_H__
