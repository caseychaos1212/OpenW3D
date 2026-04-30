/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "coopinventory.h"
#include "definitionmgr.h"
#include "gameobjmanager.h"
#include "gametype.h"
#include "powerup.h"
#include "soldier.h"
#include "smartgameobj.h"
#include "vector.h"
#include "weaponbag.h"
#include "weapons.h"

struct CoopWeaponGrant
{
	int WeaponID;
	int Rounds;
	bool HasWeapon;

	bool operator == (const CoopWeaponGrant &rhs) const
	{
		return WeaponID == rhs.WeaponID && Rounds == rhs.Rounds && HasWeapon == rhs.HasWeapon;
	}

	bool operator != (const CoopWeaponGrant &rhs) const
	{
		return !(*this == rhs);
	}
};

static DynamicVectorClass<int> _SharedPowerUps;
static DynamicVectorClass<CoopWeaponGrant> _SharedWeaponGrants;
static int _KnownSharedKeyMask = 0;
static int _SharedKeyMask = 0;
static bool IsApplying = false;

void CoopInventoryManager::Reset(void)
{
	_SharedPowerUps.Delete_All();
	_SharedWeaponGrants.Delete_All();
	_KnownSharedKeyMask = 0;
	_SharedKeyMask = 0;
	IsApplying = false;
}

bool CoopInventoryManager::Is_Applying(void)
{
	return IsApplying;
}

bool CoopInventoryManager::Should_Share_With(SoldierGameObj *soldier)
{
	return IS_COOP_MISSION &&
		soldier != NULL &&
		!soldier->Is_Delete_Pending() &&
		soldier->Is_Human_Controlled();
}

void CoopInventoryManager::Apply_Shared_Keys(SoldierGameObj *soldier)
{
	for (int key = 0; key < 31; key++) {
		int key_mask = (1 << key);
		if ((_KnownSharedKeyMask & key_mask) == 0) {
			continue;
		}

		if ((_SharedKeyMask & key_mask) != 0) {
			soldier->Give_Key(key);
		} else {
			soldier->Remove_Key(key);
		}
	}
}

void CoopInventoryManager::Apply_To_Player(SoldierGameObj *soldier)
{
	if (!Should_Share_With(soldier)) {
		return;
	}

	IsApplying = true;

	for (int index = 0; index < _SharedPowerUps.Count(); index++) {
		PowerUpGameObjDef *powerup_def = (PowerUpGameObjDef *)DefinitionMgrClass::Find_Definition(_SharedPowerUps[index]);
		if (powerup_def != NULL) {
			powerup_def->Grant_Coop_Shared(soldier, false);
		}
	}

	for (int index = 0; index < _SharedWeaponGrants.Count(); index++) {
		const CoopWeaponGrant &grant = _SharedWeaponGrants[index];
		soldier->Get_Weapon_Bag()->Add_Weapon(grant.WeaponID, grant.Rounds, grant.HasWeapon);
	}

	Apply_Shared_Keys(soldier);

	IsApplying = false;
}

void CoopInventoryManager::Share_Recorded_PowerUp(SmartGameObj *recipient, const PowerUpGameObjDef &powerup_def)
{
	IsApplying = true;

	for (SLNode<SmartGameObj> *objnode = GameObjManager::Get_Smart_Game_Obj_List()->Head(); objnode; objnode = objnode->Next()) {
		SoldierGameObj *soldier = objnode->Data()->As_SoldierGameObj();
		if (Should_Share_With(soldier) && soldier != recipient) {
			powerup_def.Grant_Coop_Shared(soldier, false);
			Apply_Shared_Keys(soldier);
		}
	}

	IsApplying = false;
}

void CoopInventoryManager::Share_Recorded_Weapon(SmartGameObj *recipient, int weapon_id, int rounds, bool has_weapon)
{
	IsApplying = true;

	for (SLNode<SmartGameObj> *objnode = GameObjManager::Get_Smart_Game_Obj_List()->Head(); objnode; objnode = objnode->Next()) {
		SoldierGameObj *soldier = objnode->Data()->As_SoldierGameObj();
		if (Should_Share_With(soldier) && soldier != recipient) {
			soldier->Get_Weapon_Bag()->Add_Weapon(weapon_id, rounds, has_weapon);
		}
	}

	IsApplying = false;
}

void CoopInventoryManager::Share_Recorded_Key(SoldierGameObj *recipient, int key, bool grant)
{
	IsApplying = true;

	for (SLNode<SmartGameObj> *objnode = GameObjManager::Get_Smart_Game_Obj_List()->Head(); objnode; objnode = objnode->Next()) {
		SoldierGameObj *soldier = objnode->Data()->As_SoldierGameObj();
		if (Should_Share_With(soldier) && soldier != recipient) {
			if (grant) {
				soldier->Give_Key(key);
			} else {
				soldier->Remove_Key(key);
			}
		}
	}

	IsApplying = false;
}

void CoopInventoryManager::Record_And_Share_PowerUp(SmartGameObj *recipient, const PowerUpGameObjDef &powerup_def)
{
	if (IsApplying || !IS_COOP_MISSION || recipient == NULL || !recipient->As_SoldierGameObj() ||
		 !recipient->As_SoldierGameObj()->Is_Human_Controlled() || !powerup_def.Has_Coop_Shared_Grant()) {
		return;
	}

	_SharedPowerUps.Add(powerup_def.Get_ID());

	int grant_key = powerup_def.Get_Grant_Key();
	if (grant_key >= 0 && grant_key < 31) {
		int key_mask = (1 << grant_key);
		_KnownSharedKeyMask |= key_mask;
		_SharedKeyMask |= key_mask;
	}

	Share_Recorded_PowerUp(recipient, powerup_def);
}

void CoopInventoryManager::Record_And_Share_Weapon_Bag(SmartGameObj *recipient, WeaponBagClass *weapon_bag)
{
	if (IsApplying || !IS_COOP_MISSION || recipient == NULL || !recipient->As_SoldierGameObj() ||
		 !recipient->As_SoldierGameObj()->Is_Human_Controlled() || weapon_bag == NULL) {
		return;
	}

	for (int index = 1; index < weapon_bag->Get_Count(); index++) {
		WeaponClass *weapon = weapon_bag->Peek_Weapon(index);
		if (weapon == NULL) {
			continue;
		}

		CoopWeaponGrant grant;
		grant.WeaponID = weapon->Get_ID();
		grant.Rounds = cGameType::Are_Coop_Ammo_Pickups_Disabled() ? 0 : weapon->Get_Total_Rounds();
		grant.HasWeapon = weapon->Does_Weapon_Exist();

		if (grant.WeaponID == 0 || (!grant.HasWeapon && grant.Rounds == 0)) {
			continue;
		}

		_SharedWeaponGrants.Add(grant);
		Share_Recorded_Weapon(recipient, grant.WeaponID, grant.Rounds, grant.HasWeapon);
	}
}

void CoopInventoryManager::Record_And_Share_Key(SoldierGameObj *recipient, int key, bool grant)
{
	if (IsApplying || !Should_Share_With(recipient) || key < 0 || key >= 31) {
		return;
	}

	int key_mask = (1 << key);
	_KnownSharedKeyMask |= key_mask;

	if (grant) {
		_SharedKeyMask |= key_mask;
	} else {
		_SharedKeyMask &= ~key_mask;
	}

	Share_Recorded_Key(recipient, key, grant);
}
