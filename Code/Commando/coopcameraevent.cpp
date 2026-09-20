/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "coopcameraevent.h"
#include "coopcamerastate.h"
#include "apppackettypes.h"
#include "ccamera.h"
#include "cnetwork.h"
#include "combat.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "gamedata.h"
#include "networkobjectfactory.h"
#include "physicalgameobj.h"
#include "scriptablegameobj.h"
#include "scriptcommands.h"
#include <cstring>

DECLARE_NETWORKOBJECT_FACTORY(cCoopCameraEvent, NETCLASSID_COOPCAMERAEVENT);

static CoopCameraState CameraState;
static unsigned int CameraSequence = 0;
static unsigned int LastCameraSequence = 0;

static void Coop_Camera_Host_Callback(GameObject *obj)
{
    if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
        return;
    }
    cCoopCameraEvent *event = new cCoopCameraEvent;
    event->Init_Camera_Host(obj);
}

static void Coop_Force_Camera_Look_Callback(const Vector3 &target)
{
    if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
        return;
    }
    cCoopCameraEvent *event = new cCoopCameraEvent;
    event->Init_Force_Look(target);
}

class CoopCameraEventRegistrationClass {
public:
    CoopCameraEventRegistrationClass() {
        ScriptCommands_Set_Camera_Host_Callback(Coop_Camera_Host_Callback);
        ScriptCommands_Set_Force_Camera_Look_Callback(Coop_Force_Camera_Look_Callback);
    }
};
static CoopCameraEventRegistrationClass CoopCameraEventRegistration;

cCoopCameraEvent::cCoopCameraEvent() :
    Sequence(0), HasForceLook(false), HostObjectId(0), Target(0.0f, 0.0f, 0.0f)
{
    Set_App_Packet_Type(APPPACKETTYPE_COOPCAMERAEVENT);
    if (The_Game() != nullptr) {
        MapName = The_Game()->Get_Map_Name();
    }
}

void cCoopCameraEvent::Init_Camera_Host(GameObject *obj)
{
    WWASSERT(cNetwork::I_Am_Server());
    CameraState.Set_Host(obj != nullptr ? obj->Get_ID() : 0);
    Capture_State();
    Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopCameraEvent::Init_Force_Look(const Vector3 &target)
{
    WWASSERT(cNetwork::I_Am_Server());
    CameraState.Set_Look(target);
    Capture_State();
    Set_Object_Dirty_Bit(BIT_CREATION, true);
}

void cCoopCameraEvent::Sync_Current_Camera_State(int client_id)
{
    if (!IS_COOP_MISSION || !cNetwork::I_Am_Server() || client_id <= 0 ||
        (cNetwork::I_Am_Client() && client_id == cNetwork::Get_My_Id())) {
        return;
    }
    cCoopCameraEvent *event = new cCoopCameraEvent;
    event->Capture_State();
    event->Set_Object_Dirty_Bit(client_id, BIT_CREATION, true);
}

void cCoopCameraEvent::Capture_State()
{
    // Every event is a complete snapshot, so a later force-look can safely
    // overtake an older host event in the object's bandwidth scheduling.
    HostObjectId = CameraState.HostId;
    HasForceLook = CameraState.HasLook;
    Target = CameraState.Look;
    Sequence = ++CameraSequence;
}

void cCoopCameraEvent::Reset(bool new_session)
{
    CameraState.Reset();
    if (new_session) {
        CameraSequence = 0;
        LastCameraSequence = 0;
    }
    // Keep sequence counters across map loads to reject delayed old events.
    if (COMBAT_CAMERA != nullptr) {
        COMBAT_CAMERA->Set_Host_Model(nullptr);
    }
    GameObjManager::Activate_Cinematic_Freeze(false);
}

void cCoopCameraEvent::Act()
{
    if (!IS_COOP_MISSION || The_Game() == nullptr ||
        std::strcmp(MapName.Peek_Buffer(), The_Game()->Get_Map_Name().Peek_Buffer()) != 0 ||
        Sequence <= LastCameraSequence) {
        return;
    }
    LastCameraSequence = Sequence;
    CameraState.Set_Host(HostObjectId);
    if (HasForceLook) {
        CameraState.Set_Look(Target);
    }
    Update();
}

void cCoopCameraEvent::Update()
{
    if (!IS_COOP_MISSION || !cNetwork::I_Am_Only_Client() || COMBAT_CAMERA == nullptr) {
        return;
    }
    if (CameraState.HostPending) {
        if (CameraState.HostId == 0) {
            COMBAT_CAMERA->Set_Host_Model(nullptr);
        } else {
            PhysicalGameObj *host = GameObjManager::Find_PhysicalGameObj(CameraState.HostId);
            if (host == nullptr || host->Is_Delete_Pending() || host->Peek_Model() == nullptr) {
                return; // Keep the desired state until the creation/model arrives.
            }
            COMBAT_CAMERA->Set_Host_Model(host->Peek_Model());
        }
        CameraState.Host_Was_Applied();
    } else if (CameraState.HostApplied) {
        PhysicalGameObj *host = GameObjManager::Find_PhysicalGameObj(CameraState.HostId);
        if (host == nullptr || host->Is_Delete_Pending()) {
            // An aborted cinematic must not retain a deleted camera model.
            COMBAT_CAMERA->Set_Host_Model(nullptr);
            CameraState.Reset();
        }
    }
    if (CameraState.LookPending) {
        COMBAT_CAMERA->Force_Look(CameraState.Look);
        CameraState.Look_Was_Applied();
    }
}

void cCoopCameraEvent::Export_Creation(BitStreamClass &packet)
{
    WWASSERT(cNetwork::I_Am_Server());
    cNetEvent::Export_Creation(packet);
    packet.Add_Terminated_String(MapName.Peek_Buffer(), true);
    packet.Add(Sequence);
    packet.Add(HasForceLook);
    packet.Add(HostObjectId);
    packet.Add(Target.X);
    packet.Add(Target.Y);
    packet.Add(Target.Z);
    Set_Delete_Pending();
}

void cCoopCameraEvent::Import_Creation(BitStreamClass &packet)
{
    WWASSERT(cNetwork::I_Am_Only_Client());
    cNetEvent::Import_Creation(packet);
    char map_name[256];
    packet.Get_Terminated_String(map_name, sizeof(map_name), true);
    MapName = map_name;
    packet.Get(Sequence);
    packet.Get(HasForceLook);
    packet.Get(HostObjectId);
    packet.Get(Target.X);
    packet.Get(Target.Y);
    packet.Get(Target.Z);
    Act();
    Set_Delete_Pending();
}
