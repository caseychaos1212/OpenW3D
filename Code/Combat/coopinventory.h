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

#ifndef COOPINVENTORY_H
#define COOPINVENTORY_H

#include "always.h"

class PowerUpGameObjDef;
class SmartGameObj;
class SoldierGameObj;
class WeaponBagClass;

class CoopInventoryManager
{
public:
	static void	Reset(void);
	static void	Apply_To_Player(SoldierGameObj *soldier);

	static void	Record_And_Share_PowerUp(SmartGameObj *recipient, const PowerUpGameObjDef &powerup_def);
	static void	Record_And_Share_Weapon_Bag(SmartGameObj *recipient, WeaponBagClass *weapon_bag);
	static void	Record_And_Share_Key(SoldierGameObj *recipient, int key, bool grant);

	static bool	Is_Applying(void);

private:
	static bool	Should_Share_With(SoldierGameObj *soldier);
	static void	Apply_Shared_Keys(SoldierGameObj *soldier);
	static void	Share_Recorded_PowerUp(SmartGameObj *recipient, const PowerUpGameObjDef &powerup_def);
	static void	Share_Recorded_Weapon(SmartGameObj *recipient, int weapon_id, int rounds, bool has_weapon);
	static void	Share_Recorded_Key(SoldierGameObj *recipient, int key, bool grant);
};

#endif
