/*
** Copyright 2026 OpenW3D Contributors.
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef COOPCAMERASTATE_H
#define COOPCAMERASTATE_H
#include "vector3.h"

// Desired camera state survives an event's deletion while its model is loading.
// A new host/release replaces outstanding work, so a delayed model cannot
// restore a cinematic that has already ended.
class CoopCameraState {
public:
    void Reset() { *this = CoopCameraState(); }
    void Set_Host(int id) {
        HostId = id;
        HostPending = true;
        HostApplied = false;
        HasLook = false;
        LookPending = false;
    }
    void Set_Look(const Vector3 &target) {
        Look = target;
        HasLook = true;
        LookPending = true;
    }
    void Host_Was_Applied() { HostPending = false; HostApplied = HostId > 0; }
    void Look_Was_Applied() { LookPending = false; }

    int HostId = 0;
    bool HostPending = false;
    bool HostApplied = false;
    bool HasLook = false;
    bool LookPending = false;
    Vector3 Look = Vector3(0.0f, 0.0f, 0.0f);
};
#endif
