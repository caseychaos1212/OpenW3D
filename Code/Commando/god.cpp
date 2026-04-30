/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//
// Filename:     god.cpp
// Author:       Tom Spencer-Smith
// Date:         Dec 1998
// Description:  This class handles the creations of players and soldiers.
//

#include "god.h"

#include "cnetwork.h"
#include "playermanager.h"
#include "gameobjmanager.h"
#include "basegameobj.h"
#include "spawn.h"
#include "gdcoopmission.h"
#include "coopdebuglog.h"
#include "coopinventory.h"
#include "coopcameraevent.h"
#include "cooprespawnstateevent.h"
#include "crandom.h"
#include "playertype.h"
#include "objlibrary.h"
#include "definitionmgr.h"
#include "combatchunkid.h"
#include "phys.h"
#include "humanphys.h"
#include "soldier.h"
#include "soldierobserver.h"
#include "gametype.h"
#include "dialogtests.h"
#include "combatgmode.h"
#include "gameinitmgr.h"
#include "scriptman.h"
#include "debug.h"
#include "renegadedialogmgr.h"
#include "cheatmgr.h"
#include "wwmemlog.h"
#include "dialogmgr.h"
#include "encyclopediamgr.h"
#include "wolgmode.h"
#include "specialbuilds.h"
#include "demosupport.h"

/*
**
*/
typedef enum {
	GOD_STATE_UNINITIALIZED,
	GOD_STATE_MULTIPLAYER,
	GOD_STATE_EXITING,

	GOD_STATE_SINGLE_INIT,
	GOD_STATE_SINGLE_RUNNING,
	GOD_STATE_SINGLE_DEAD,

	GOD_STATE_COOP_INIT,
	GOD_STATE_COOP_RUNNING,

} GodState;

int		cGod::State		= GOD_STATE_UNINITIALIZED;
InventoryClass	cGod::LevelStartInventory;

struct CoopCharacterPresetSelection
{
	int ClientId;
	StringClass PresetName;
};

static const int MAX_COOP_CHARACTER_PRESET_SELECTIONS = MAX_PLAYERS;
static CoopCharacterPresetSelection CoopCharacterPresetSelections[MAX_COOP_CHARACTER_PRESET_SELECTIONS];
static int CoopCharacterPresetSelectionCount = 0;

struct CoopRespawnWaitState
{
	int ClientId;
	bool Waiting;
	int SpectateObjectId;
};

static const int MAX_COOP_RESPAWN_WAIT_STATES = MAX_PLAYERS;
static CoopRespawnWaitState CoopRespawnWaitStates[MAX_COOP_RESPAWN_WAIT_STATES];
static int CoopRespawnWaitStateCount = 0;

static const char * COOP_GENERIC_GDI_PRESETS[] = {
	"GDI_MiniGunner_0",
	"GDI_MiniGunner_1Off",
	"GDI_Grenadier_0",
	"GDI_RocketSoldier_0",
	"GDI_Engineer_0"
};

//-----------------------------------------------------------------------------
static cPlayer * Get_First_Active_In_Game_Player(void)
{
	for (
		SLNode<cPlayer> * objnode = cPlayerManager::Get_Player_Object_List()->Head();
		objnode;
		objnode = objnode->Next()) {

		cPlayer * p_player = objnode->Data();
		if (p_player != NULL &&
			 p_player->Get_Is_Active().Is_True() &&
			 p_player->Get_Is_In_Game().Is_True()) {
			return p_player;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
static int Get_Coop_Player_Index(int client_id)
{
	int player_index = 0;
	for (
		SLNode<cPlayer> * objnode = cPlayerManager::Get_Player_Object_List()->Head();
		objnode;
		objnode = objnode->Next()) {

		cPlayer * p_player = objnode->Data();
		if (p_player == NULL ||
			 p_player->Get_Is_Active().Is_False() ||
			 p_player->Get_Is_In_Game().Is_False()) {
			continue;
		}

		if (p_player->Get_Id() == client_id) {
			return player_index;
		}

		player_index++;
	}

	return -1;
}

//-----------------------------------------------------------------------------
static bool Is_Coop_Secondary_Player(int client_id)
{
	if (!IS_COOP_MISSION) {
		return false;
	}

	return Get_Coop_Player_Index(client_id) > 0;
}

//-----------------------------------------------------------------------------
static bool Is_Living_Coop_Player_Soldier(SoldierGameObj * soldier)
{
	return soldier != NULL &&
		!soldier->Is_Delete_Pending() &&
		!soldier->Is_Dead() &&
		soldier->Get_Defense_Object()->Get_Health() > 0.0F;
}

//-----------------------------------------------------------------------------
static int Find_Coop_Respawn_Wait_State(int client_id)
{
	for (int index = 0; index < CoopRespawnWaitStateCount; index++) {
		if (CoopRespawnWaitStates[index].ClientId == client_id) {
			return index;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
static int Get_Coop_Respawn_Wait_State(int client_id)
{
	int index = Find_Coop_Respawn_Wait_State(client_id);
	if (index < 0 && CoopRespawnWaitStateCount < MAX_COOP_RESPAWN_WAIT_STATES) {
		index = CoopRespawnWaitStateCount++;
		CoopRespawnWaitStates[index].ClientId = client_id;
		CoopRespawnWaitStates[index].Waiting = false;
		CoopRespawnWaitStates[index].SpectateObjectId = 0;
	}

	return index;
}

//-----------------------------------------------------------------------------
static bool Is_Coop_Respawn_Waiting_For(int client_id, SoldierGameObj * spectate_soldier)
{
	if (spectate_soldier == NULL) {
		return false;
	}

	int index = Find_Coop_Respawn_Wait_State(client_id);
	return index >= 0 &&
		CoopRespawnWaitStates[index].Waiting &&
		CoopRespawnWaitStates[index].SpectateObjectId == spectate_soldier->Get_ID();
}

//-----------------------------------------------------------------------------
static void Set_Coop_Respawn_Waiting(int client_id, SoldierGameObj * spectate_soldier)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server() || spectate_soldier == NULL) {
		return;
	}

	int spectate_object_id = spectate_soldier->Get_ID();
	int index = Get_Coop_Respawn_Wait_State(client_id);
	if (index >= 0 &&
		 CoopRespawnWaitStates[index].Waiting &&
		 CoopRespawnWaitStates[index].SpectateObjectId == spectate_object_id) {
		return;
	}

	if (index >= 0) {
		CoopRespawnWaitStates[index].Waiting = true;
		CoopRespawnWaitStates[index].SpectateObjectId = spectate_object_id;
	}

	CoopDebugLog::Log("cGod::Create_Commando waiting for safe ally spawn client_id=%d spectate_object_id=%d",
		client_id, spectate_object_id);

	cCoopRespawnStateEvent *event = new cCoopRespawnStateEvent;
	event->Init(client_id, true, spectate_object_id);
}

//-----------------------------------------------------------------------------
static void Clear_Coop_Respawn_Waiting(int client_id)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return;
	}

	int index = Find_Coop_Respawn_Wait_State(client_id);
	if (index < 0 || !CoopRespawnWaitStates[index].Waiting) {
		return;
	}

	CoopRespawnWaitStates[index].Waiting = false;
	CoopRespawnWaitStates[index].SpectateObjectId = 0;

	cCoopRespawnStateEvent *event = new cCoopRespawnStateEvent;
	event->Init(client_id, false, 0);
}

//-----------------------------------------------------------------------------
static void Reset_Coop_Respawn_Waiting(void)
{
	CoopRespawnWaitStateCount = 0;
}

//-----------------------------------------------------------------------------
static SoldierGameObj * Find_Living_Coop_Ally(int client_id)
{
	for (
		SLNode<cPlayer> * objnode = cPlayerManager::Get_Player_Object_List()->Head();
		objnode;
		objnode = objnode->Next()) {

		cPlayer * p_player = objnode->Data();
		if (p_player == NULL ||
			 p_player->Get_Id() == client_id ||
			 p_player->Get_Is_Active().Is_False() ||
			 p_player->Get_Is_In_Game().Is_False()) {
			continue;
		}

		SmartGameObj * smart_soldier = GameObjManager::Find_Soldier_Of_Client_ID(p_player->Get_Id());
		SoldierGameObj * soldier = smart_soldier != NULL ? smart_soldier->As_SoldierGameObj() : NULL;
		if (Is_Living_Coop_Player_Soldier(soldier)) {
			return soldier;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
static bool Is_Hostile_Near_Soldier(SoldierGameObj * soldier)
{
	WWASSERT(soldier != NULL);

	static const float COOP_ALLY_RESPAWN_ENEMY_RADIUS = 30.0F;
	static const float COOP_ALLY_RESPAWN_ENEMY_RADIUS_SQ =
		COOP_ALLY_RESPAWN_ENEMY_RADIUS * COOP_ALLY_RESPAWN_ENEMY_RADIUS;

	Vector3 soldier_pos;
	soldier->Get_Position(&soldier_pos);

	for (
		SLNode<BaseGameObj> * objnode = GameObjManager::Get_Game_Obj_List()->Head();
		objnode;
		objnode = objnode->Next()) {

		BaseGameObj * base_obj = objnode->Data();
		if (base_obj == NULL || base_obj->Is_Delete_Pending()) {
			continue;
		}

		PhysicalGameObj * physical_obj = base_obj->As_PhysicalGameObj();
		if (physical_obj == NULL ||
			 physical_obj == soldier ||
			 physical_obj->Is_Delete_Pending() ||
			 physical_obj->Get_Defense_Object()->Get_Health() <= 0.0F ||
			 !soldier->Is_Enemy(physical_obj)) {
			continue;
		}

		Vector3 obj_pos;
		physical_obj->Get_Position(&obj_pos);

		Vector3 delta = obj_pos - soldier_pos;
		float distance_sq = delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
		if (distance_sq <= COOP_ALLY_RESPAWN_ENEMY_RADIUS_SQ) {
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
struct CoopSpawnOffset
{
	float Forward;
	float Side;
};

//-----------------------------------------------------------------------------
static Matrix3D Build_Coop_Spawn_Test_Transform(const Matrix3D & base_transform, const CoopSpawnOffset & offset, float height_offset)
{
	Matrix3D test_transform = base_transform;
	Vector3 position = base_transform.Get_Translation();
	position += base_transform.Get_X_Vector() * offset.Forward;
	position += base_transform.Get_Y_Vector() * offset.Side;
	position.Z += height_offset;
	test_transform.Set_Translation(position);
	return test_transform;
}

//-----------------------------------------------------------------------------
static bool Test_Coop_Spawn_Transform(SoldierGameObj * soldier, const Matrix3D & test_transform, Matrix3D & safe_transform)
{
	WWASSERT(soldier != NULL);

	HumanPhysClass * human_phys = soldier->Peek_Human_Phys();
	if (human_phys == NULL) {
		safe_transform = test_transform;
		return true;
	}

	return human_phys->Can_Teleport_And_Stand(test_transform, &safe_transform);
}

//-----------------------------------------------------------------------------
static bool Find_Safe_Coop_Spawn_Transform(SoldierGameObj * soldier, const Matrix3D & base_transform, bool include_center, Matrix3D & safe_transform)
{
	static const float COOP_SPAWN_TEST_HEIGHT_OFFSET = 1.0F;
	static const CoopSpawnOffset OFFSETS[] = {
		{ 0.0F, 1.5F },
		{ 0.0F, -1.5F },
		{ -1.5F, 0.0F },
		{ 1.5F, 0.0F },
		{ -1.5F, 1.5F },
		{ -1.5F, -1.5F },
		{ 1.5F, 1.5F },
		{ 1.5F, -1.5F },
		{ 0.0F, 2.5F },
		{ 0.0F, -2.5F },
		{ -2.5F, 0.0F },
		{ 2.5F, 0.0F },
		{ -2.5F, 2.5F },
		{ -2.5F, -2.5F },
		{ 2.5F, 2.5F },
		{ 2.5F, -2.5F },
		{ 0.0F, 4.0F },
		{ 0.0F, -4.0F },
		{ -4.0F, 0.0F },
		{ 4.0F, 0.0F }
	};

	if (include_center && Test_Coop_Spawn_Transform(soldier, base_transform, safe_transform)) {
		return true;
	}

	if (include_center) {
		Matrix3D raised_center = Build_Coop_Spawn_Test_Transform(base_transform, { 0.0F, 0.0F }, COOP_SPAWN_TEST_HEIGHT_OFFSET);
		if (Test_Coop_Spawn_Transform(soldier, raised_center, safe_transform)) {
			return true;
		}
	}

	for (int index = 0; index < (int)(sizeof(OFFSETS) / sizeof(OFFSETS[0])); index++) {
		Matrix3D test_transform = Build_Coop_Spawn_Test_Transform(base_transform, OFFSETS[index], COOP_SPAWN_TEST_HEIGHT_OFFSET);
		if (Test_Coop_Spawn_Transform(soldier, test_transform, safe_transform)) {
			return true;
		}
	}

	HumanPhysClass * human_phys = soldier->Peek_Human_Phys();
	if (human_phys != NULL) {
		Vector3 safe_position;
		if (human_phys->Find_Teleport_Location(base_transform.Get_Translation(), 4.0F, &safe_position)) {
			safe_transform = base_transform;
			safe_transform.Set_Translation(safe_position);
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
static bool Try_Get_Coop_Ally_Spawn_Transform(int client_id, SoldierGameObj * soldier, Matrix3D & transform, SoldierGameObj ** wait_for_ally)
{
	if (wait_for_ally != NULL) {
		*wait_for_ally = NULL;
	}

	SoldierGameObj * ally = Find_Living_Coop_Ally(client_id);
	if (ally == NULL) {
		return false;
	}

	if (Is_Hostile_Near_Soldier(ally)) {
		if (!Is_Coop_Respawn_Waiting_For(client_id, ally)) {
			CoopDebugLog::Log("cGod::Create_Commando ally spawn blocked by nearby hostile client_id=%d ally_net_id=%d",
				client_id, ally->Get_Network_ID());
		}
		if (wait_for_ally != NULL) {
			*wait_for_ally = ally;
		}
		return false;
	}

	if (Find_Safe_Coop_Spawn_Transform(soldier, ally->Get_Transform(), false, transform)) {
		Vector3 position = transform.Get_Translation();
		CoopDebugLog::Log("cGod::Create_Commando ally spawn selected client_id=%d ally_net_id=%d pos=(%.2f, %.2f, %.2f)",
			client_id, ally->Get_Network_ID(), position.X, position.Y, position.Z);
		return true;
	}

	if (!Is_Coop_Respawn_Waiting_For(client_id, ally)) {
		CoopDebugLog::Log("cGod::Create_Commando ally spawn failed safety checks client_id=%d ally_net_id=%d",
			client_id, ally->Get_Network_ID());
	}
	if (wait_for_ally != NULL) {
		*wait_for_ally = ally;
	}
	return false;
}

//-----------------------------------------------------------------------------
static bool Get_Coop_Mission_Spawn_Transform(int client_id, SoldierGameObj * soldier, bool prefer_ally_spawn, Matrix3D & transform, SoldierGameObj ** wait_for_ally)
{
	if (wait_for_ally != NULL) {
		*wait_for_ally = NULL;
	}

	if (prefer_ally_spawn) {
		Matrix3D ally_transform;
		SoldierGameObj * waiting_ally = NULL;
		if (Try_Get_Coop_Ally_Spawn_Transform(client_id, soldier, ally_transform, &waiting_ally)) {
			transform = ally_transform;
			return true;
		}
		if (waiting_ally != NULL) {
			if (wait_for_ally != NULL) {
				*wait_for_ally = waiting_ally;
			}
			return false;
		}
	}

	Matrix3D primary_transform = SpawnManager::Get_Primary_Spawn_Location();
	Matrix3D safe_transform;
	bool include_center = !Is_Coop_Secondary_Player(client_id);
	if (Find_Safe_Coop_Spawn_Transform(soldier, primary_transform, include_center, safe_transform)) {
		transform = safe_transform;
		return true;
	}

	if (!include_center && Find_Safe_Coop_Spawn_Transform(soldier, primary_transform, true, safe_transform)) {
		transform = safe_transform;
		return true;
	}

	CoopDebugLog::Log("cGod::Create_Commando no safe co-op spawn found; using primary spawn client_id=%d", client_id);
	transform = primary_transform;
	return true;
}

//-----------------------------------------------------------------------------
static bool Get_Preset_Model_Name(const StringClass & preset_name, StringClass & model_name)
{
	DefinitionClass * object_base_def = DefinitionMgrClass::Find_Typed_Definition(preset_name, CLASSID_GAME_OBJECTS);
	PhysicalGameObjDef * object_def = (PhysicalGameObjDef *)object_base_def;
	if (object_def == NULL) {
		return false;
	}

	DefinitionClass * phys_base_def = DefinitionMgrClass::Find_Definition(object_def->Get_Phys_Def_ID());
	PhysDefClass * phys_def = (PhysDefClass *)phys_base_def;
	if (phys_def == NULL || phys_def->Get_Model_Name().Is_Empty()) {
		return false;
	}

	model_name = phys_def->Get_Model_Name();
	return true;
}

//-----------------------------------------------------------------------------
static int Find_Coop_Character_Preset_Selection(int client_id)
{
	for (int index = 0; index < CoopCharacterPresetSelectionCount; index++) {
		if (CoopCharacterPresetSelections[index].ClientId == client_id) {
			return index;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
static bool Get_Coop_Character_Preset(int client_id, StringClass & preset_name)
{
	int index = Find_Coop_Character_Preset_Selection(client_id);
	if (index < 0) {
		return false;
	}

	preset_name = CoopCharacterPresetSelections[index].PresetName;
	return !preset_name.Is_Empty();
}

//-----------------------------------------------------------------------------
static void Store_Coop_Character_Preset(int client_id, const StringClass & preset_name)
{
	int index = Find_Coop_Character_Preset_Selection(client_id);
	if (index < 0) {
		if (CoopCharacterPresetSelectionCount < MAX_COOP_CHARACTER_PRESET_SELECTIONS) {
			index = CoopCharacterPresetSelectionCount++;
		} else {
			index = 0;
		}
	}

	CoopCharacterPresetSelections[index].ClientId = client_id;
	CoopCharacterPresetSelections[index].PresetName = preset_name;
}

//-----------------------------------------------------------------------------
static bool Is_Valid_Coop_Character_Preset(const StringClass & preset_name)
{
	if (preset_name.Is_Empty()) {
		return false;
	}

	return DefinitionMgrClass::Find_Typed_Definition(
		preset_name.Peek_Buffer(),
		CLASSID_GAME_OBJECT_DEF_SOLDIER) != NULL;
}

//-----------------------------------------------------------------------------
static bool Choose_Random_Generic_GDI_Preset(StringClass & preset_name)
{
	const int count = sizeof(COOP_GENERIC_GDI_PRESETS) / sizeof(COOP_GENERIC_GDI_PRESETS[0]);
	const int start = FreeRandom.Get_Int(count);

	for (int offset = 0; offset < count; offset++) {
		const char * candidate = COOP_GENERIC_GDI_PRESETS[(start + offset) % count];
		StringClass candidate_name(candidate, true);
		if (Is_Valid_Coop_Character_Preset(candidate_name)) {
			preset_name = candidate_name;
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
static bool Get_Default_Coop_Character_Preset(int client_id, StringClass & preset_name)
{
	if (!IS_COOP_MISSION) {
		return false;
	}

	const int player_index = Get_Coop_Player_Index(client_id);
	if (player_index < 1) {
		return false;
	}

	if (player_index == 1) {
		cGameDataCoopMission * coop_game = The_Game()->As_Coop_Mission();
		WWASSERT(coop_game != NULL);

		const StringClass & player2_preset = coop_game->Get_Player2_Preset();
		if (!player2_preset.Is_Empty()) {
			preset_name = player2_preset;
			return true;
		}

		return false;
	}

	if (Choose_Random_Generic_GDI_Preset(preset_name)) {
		Store_Coop_Character_Preset(client_id, preset_name);
		CoopDebugLog::Log("cGod::Create_Commando default generic GDI character client_id=%d preset=%s",
			client_id, preset_name.Peek_Buffer());
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
static bool Apply_Coop_Character_Preset(SoldierGameObj * soldier, const StringClass & preset_name)
{
	if (soldier == NULL || !Is_Valid_Coop_Character_Preset(preset_name)) {
		return false;
	}

	StringClass model_name;
	if (!Get_Preset_Model_Name(preset_name, model_name)) {
		return false;
	}

	soldier->Set_Model(model_name.Peek_Buffer());
	soldier->Set_Object_Dirty_Bit(NetworkObjectClass::BIT_RARE, true);
	return true;
}

//-----------------------------------------------------------------------------
static void Attach_Mission_Start_Script(SoldierGameObj * soldier)
{
	WWASSERT(soldier != NULL);

	const char * script_name = CombatManager::Get_Start_Script();
	if (script_name != NULL && script_name[0] != 0) {
		ScriptClass* script = ScriptManager::Create_Script( script_name );
		if (script) {
			soldier->Add_Observer( script );
		}
	}
}

//-----------------------------------------------------------------------------
enum	{
	CHUNKID_VARIABLES = 112374,

	MICROCHUNK_STATE = 1,
};

//-----------------------------------------------------------------------------
bool cGod::Save(ChunkSaveClass & csave)
{
	csave.Begin_Chunk(CHUNKID_VARIABLES);
	WRITE_MICRO_CHUNK(csave, MICROCHUNK_STATE, State);
	csave.End_Chunk();
	return true;
}

//-----------------------------------------------------------------------------
bool cGod::Load(ChunkLoadClass &cload)
{
	while (cload.Open_Chunk()) {
		switch(cload.Cur_Chunk_ID()) {

			case CHUNKID_VARIABLES:
				while (cload.Open_Micro_Chunk()) {
					switch(cload.Cur_Micro_Chunk_ID()) {
						READ_MICRO_CHUNK(cload, MICROCHUNK_STATE, State);
						default:
							Debug_Say(( "Unrecognized cGod Variable chunkID\n" ));
							break;
					}
					cload.Close_Micro_Chunk();
				}
				break;

			default:
				Debug_Say(( "Unrecognized cPlayer chunkID\n" ));
				break;
		}
		cload.Close_Chunk();
	}

	return true;
}


//-----------------------------------------------------------------------------
void cGod::Think(void)
{
	WWASSERT(cNetwork::I_Am_Server());

	//if (The_Game()->IsIntermission.Is_True()) {
	WWASSERT(PTheGameData != NULL);
	if (The_Game()->IsIntermission.Is_True() ||
		 cPlayerManager::Get_Player_Object_List()->Head() == NULL) {
		return;
	}

	if ( State == GOD_STATE_UNINITIALIZED ) {
		//XXX
		if (IS_COOP_MISSION) {
			CoopDebugLog::Log("cGod::Think transition UNINITIALIZED -> COOP_INIT");
			State = GOD_STATE_COOP_INIT;
		} else {
			State = ( IS_MISSION ) ? GOD_STATE_SINGLE_INIT : GOD_STATE_MULTIPLAYER;
		}
	}

	if ( State == GOD_STATE_SINGLE_INIT ) {

		WWASSERT( cPlayerManager::Get_Player_Object_List()->Head() != NULL );
		// Create a Commando for the Player
		SoldierGameObj * soldier = Create_Commando( cPlayerManager::Get_Player_Object_List()->Head()->Data() );

		Attach_Mission_Start_Script(soldier);

		State = GOD_STATE_SINGLE_RUNNING;
	}

	if ( State == GOD_STATE_COOP_INIT ) {

		cPlayer * first_player = Get_First_Active_In_Game_Player();
		if (first_player == NULL) {
			CoopDebugLog::Log("cGod::Think COOP_INIT waiting for first active in-game player");
			return;
		}

		CoopDebugLog::Log("cGod::Think COOP_INIT spawning first player id=%d", first_player->Get_Id());
		CoopInventoryManager::Reset();

		SoldierGameObj * soldier = Create_Commando(first_player);
		CoopDebugLog::Log("cGod::Think COOP_INIT first player soldier=%p net_id=%d",
			soldier, soldier != NULL ? soldier->Get_Network_ID() : 0);
		Attach_Mission_Start_Script(soldier);

		CoopDebugLog::Log("cGod::Think transition COOP_INIT -> COOP_RUNNING");
		State = GOD_STATE_COOP_RUNNING;
	}

	// This code may need to get cleaned up
	if ( State == GOD_STATE_SINGLE_RUNNING ) {
		//If we just loaded, we may have a play and a solder, but they will not be linked.
		if (cPlayerManager::Get_Player_Object_List() != NULL &&
			 cPlayerManager::Get_Player_Object_List()->Head() != NULL ) {

			cPlayer * p_player = cPlayerManager::Get_Player_Object_List()->Head()->Data();
			if ( p_player != NULL ) {
		   		SmartGameObj * p_soldier = GameObjManager::Find_Soldier_Of_Client_ID(p_player->Get_Id());
				if ( p_soldier && p_player->Get_GameObj() == NULL ) {
					// Remap
					p_soldier->Set_Player_Data( p_player );
					Debug_Say(( "Fixing up player data after load\n" ));
				}
			}
		}
	}

	DEMO_SECURITY_CHECK;

   //
   // Take a look through the player list and create commando bodies
	// for anyone who merits one
   //
	if ( State == GOD_STATE_MULTIPLAYER || State == GOD_STATE_COOP_RUNNING ) {
		for (
			SLNode<cPlayer> * objnode = cPlayerManager::Get_Player_Object_List()->Head();
			objnode;
			objnode = objnode->Next()) {

			cPlayer * p_player = objnode->Data();
			WWASSERT(p_player != NULL);

			if (p_player->Get_Is_Active().Is_False()) {
				continue;
			}

			if (p_player->Get_Is_In_Game().Is_False()) {
				continue;
			}

			SmartGameObj * p_soldier = GameObjManager::Find_Soldier_Of_Client_ID(p_player->Get_Id());

			if (p_soldier == NULL) {
				//
				// A disembodied player... give him a body
				//
				if (State == GOD_STATE_COOP_RUNNING) {
					CoopDebugLog::Log("cGod::Think COOP_RUNNING spawning missing body for player id=%d", p_player->Get_Id());
				}
				SoldierGameObj * soldier = Create_Commando(p_player, State == GOD_STATE_COOP_RUNNING);
				if (State == GOD_STATE_COOP_RUNNING) {
					if (soldier != NULL) {
						cCoopCameraEvent::Sync_Current_Camera_State();
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
cPlayer * cGod::Create_Player(int client_id, const WideStringClass & name,
															int team_choice, unsigned int clanID, bool is_invulnerable)
{
	WWMEMLOG(MEM_NETWORK);
	WWASSERT(cNetwork::I_Am_Server());
	WWASSERT(PTheGameData != NULL);
	WWASSERT(cPlayerManager::Count() < The_Game()->Get_Max_Players());

   //
	// Assign a player type
	//
	cPlayer * p_player = cPlayerManager::Find_Player(name);

	if (p_player != NULL) {
		//
		// I think this can happen when a player crashes out and then rejoins
		// before the server breaks his connection...
		//
		cNetwork::Delete_Player_Objects(p_player->Get_Id());
	} else {
		p_player = cPlayerManager::Find_Inactive_Player(name);
	}

	bool is_new = false;

	if (p_player == NULL) {
      p_player = new cPlayer();
      WWASSERT(p_player != NULL);
		is_new = true;

		p_player->Set_Name(name);
		p_player->Set_Id(client_id);
		p_player->Set_WOL_ClanID(clanID);

		int player_type = The_Game()->Choose_Player_Type(p_player, team_choice, false);
		p_player->Set_Player_Type(player_type);
	} else {
		p_player->Set_Id(client_id);
		p_player->Set_Is_In_Game(true);
		p_player->Set_Is_Waiting_For_Intermission(false);
	}

	p_player->Reset_Join_Time();
	p_player->Invulnerable.Set(is_invulnerable);
	p_player->Set_Is_Active(true);
	if (IS_COOP_MISSION) {
		CoopDebugLog::Log("cGod::Create_Player id=%d new=%d team_choice=%d player_type=%d invulnerable=%d",
			client_id, is_new, team_choice, p_player->Get_Player_Type(), is_invulnerable);
	}

	//
	// Tell everyone about this guy
	//
	if (is_new) {
		p_player->Init();

		GameModeClass* gameMode = GameModeManager::Find("WOL");

		if (gameMode && gameMode->Is_Active()) {
			WolGameModeClass* wolGame = reinterpret_cast<WolGameModeClass*>(gameMode);
			wolGame->Init_WOL_Player(p_player);
		}
	}

	return p_player;
}

//-----------------------------------------------------------------------------
void cGod::Create_Ai_Player(void)
{
	WWASSERT(PTheGameData != NULL);
	WWASSERT(cPlayerManager::Count() < The_Game()->Get_Max_Players());
   WWASSERT(cNetwork::I_Am_Server());

	//
	// For id, count downwards from -2.
	//
	int client_id = -1;//SmartGameObj::MOBIUS_CONTROL_OWNER;
	WideStringClass name;
	do {
		client_id--;
		WWASSERT(client_id > cPlayer::INVALID_ID);
		name.Format(U_CHAR("Guard%d"), -client_id);
	} while (cPlayerManager::Is_Player_Present(name));

	Create_Player(client_id, name, -1, 0);
}

//-----------------------------------------------------------------------------
SoldierGameObj * cGod::Create_Commando(int client_id, int player_type, bool prefer_ally_spawn/*, int model_num*/)
{
   WWASSERT(cNetwork::I_Am_Server());
	WWASSERT(player_type >= PLAYERTYPE_NEUTRAL && player_type <= PLAYERTYPE_LAST);

	WWASSERT(PTheGameData != NULL);
	if (IS_COOP_MISSION) {
		CoopDebugLog::Log("cGod::Create_Commando begin client_id=%d player_type=%d", client_id, player_type);
	}

	if (IS_COOP_MISSION && prefer_ally_spawn && player_type == PLAYERTYPE_GDI) {
		SoldierGameObj * ally = Find_Living_Coop_Ally(client_id);
		if (ally != NULL && Is_Hostile_Near_Soldier(ally)) {
			if (!Is_Coop_Respawn_Waiting_For(client_id, ally)) {
				CoopDebugLog::Log("cGod::Create_Commando ally spawn blocked by nearby hostile client_id=%d ally_net_id=%d",
					client_id, ally->Get_Network_ID());
			}
			Set_Coop_Respawn_Waiting(client_id, ally);
			return NULL;
		}
	}

	StringClass preset_name;
	preset_name.Format("Commando");
	StringClass primary_mission_preset;
	StringClass coop_model_preset;

	if (IS_MISSION) {

#ifndef MULTIPLAYERDEMO
		SpawnerClass * p_spawner = SpawnManager::Get_Primary_Spawner();
		if (p_spawner != NULL) {
			const DynamicVectorClass<int>	& def_list = p_spawner->Get_Definition().Get_Spawn_Definition_ID_List();
			WWASSERT(def_list.Count() >= 1);
			PhysicalGameObjDef * p_def = (PhysicalGameObjDef *)DefinitionMgrClass::Find_Definition(def_list[0]);
			if (p_def != NULL) {
				preset_name.Format("%s", p_def->Get_Name());
			}
		}
#endif // !MULTIPLAYERDEMO
		primary_mission_preset = preset_name;

		if (IS_COOP_MISSION && player_type == PLAYERTYPE_GDI) {
			if (Get_Coop_Character_Preset(client_id, coop_model_preset)) {
				CoopDebugLog::Log("cGod::Create_Commando character selection client_id=%d preset=%s",
					client_id, coop_model_preset.Peek_Buffer());
			} else if (!Get_Default_Coop_Character_Preset(client_id, coop_model_preset) && Is_Coop_Secondary_Player(client_id)) {
				Debug_Say(("Co-op secondary player preset is not configured; falling back to %s\n", primary_mission_preset.Peek_Buffer()));
			}
		}

	} else if (The_Game()->Is_Cnc() || The_Game()->Is_Skirmish()) {
		if (player_type == PLAYERTYPE_NOD) {
			preset_name.Format("CnC_Nod_Minigunner_0");
		} else {
			preset_name.Format("CnC_GDI_MiniGunner_0");
		}
	}

	WWASSERT(!preset_name.Is_Empty());
	PhysicalGameObj * p_phys_obj = ObjectLibraryManager::Create_Object(preset_name);
	WWASSERT(p_phys_obj != NULL);
	if (IS_COOP_MISSION) {
		CoopDebugLog::Log("cGod::Create_Commando object created client_id=%d preset=%s object=%p net_id=%d",
			client_id, preset_name.Peek_Buffer(), p_phys_obj, p_phys_obj != NULL ? p_phys_obj->Get_Network_ID() : 0);
	}

	SoldierGameObj * p_soldier = p_phys_obj->As_SoldierGameObj();
	WWASSERT(p_soldier != NULL);
	WWASSERT(p_soldier->Peek_Physical_Object() != NULL);

	if (!coop_model_preset.Is_Empty()) {
		StringClass coop_model_name;
		if (Get_Preset_Model_Name(coop_model_preset, coop_model_name)) {
			CoopDebugLog::Log("cGod::Create_Commando model override client_id=%d preset=%s model=%s",
				client_id, coop_model_preset.Peek_Buffer(), coop_model_name.Peek_Buffer());
			p_soldier->Set_Model(coop_model_name.Peek_Buffer());
			p_soldier->Set_Object_Dirty_Bit(NetworkObjectClass::BIT_RARE, true);
		} else {
			Debug_Say(("Co-op character preset %s is invalid; using %s model\n",
				coop_model_preset.Peek_Buffer(), primary_mission_preset.Peek_Buffer()));
		}
	}

	if (IS_SOLOPLAY || IS_COOP_MISSION) {
		// Setup initial health depending on difficulty level
		float max = 100.0f;
		switch ( CombatManager::Get_Difficulty_Level() ) {
			case 0:	max = 200;	break;
			case 1:	max = 100;	break;
			case 2:	max = 75;	break;
		};
		p_soldier->Get_Defense_Object()->Set_Health_Max( max );
		p_soldier->Get_Defense_Object()->Set_Health( max );
		p_soldier->Get_Defense_Object()->Set_Shield_Strength_Max( max );
		p_soldier->Get_Defense_Object()->Set_Shield_Strength( max );
	}

	Matrix3D transform;
	if (IS_MISSION && player_type == PLAYERTYPE_GDI) {
		if (IS_COOP_MISSION) {
			SoldierGameObj * wait_for_ally = NULL;
			if (!Get_Coop_Mission_Spawn_Transform(client_id, p_soldier, prefer_ally_spawn, transform, &wait_for_ally)) {
				Set_Coop_Respawn_Waiting(client_id, wait_for_ally);
				p_soldier->Set_Delete_Pending();
				return NULL;
			}
		} else {
			transform = SpawnManager::Get_Primary_Spawn_Location();
		}
	} else {
		transform = SpawnManager::Get_Multiplayer_Spawn_Location(player_type,p_soldier);
	}
	if (IS_COOP_MISSION) {
		Clear_Coop_Respawn_Waiting(client_id);
	}
	p_soldier->Set_Transform(transform);
	if (IS_COOP_MISSION) {
		Vector3 position = transform.Get_Translation();
		CoopDebugLog::Log("cGod::Create_Commando transform client_id=%d pos=(%.2f, %.2f, %.2f)",
			client_id, position.X, position.Y, position.Z);
	}

	p_soldier->Set_Control_Owner(client_id);
	cPlayer * player = cPlayerManager::Find_Player( client_id );
	p_soldier->Set_Player_Data( player );

	if ( The_Game()->Remember_Inventory() ) {
		if (IS_MISSION) {
			cGod::Restore_Inventory( p_soldier );
		}
	}

	p_soldier->Set_Player_Type(player_type);

	//
	// TSS082901 - We cannot remove all observers - there may be scripts granting
	// initial weapons. Instead, make sure the presets don't UseInnateBehavior
	//
	//p_soldier->Remove_All_Observers();

	if (!p_soldier->Is_Human_Controlled()) {
		//
		// Add an observer to do innate AI
		//
		p_soldier->Set_Innate_Observer(new SoldierObserverClass);
		p_soldier->Add_Observer(p_soldier->Get_Innate_Observer());
	}

	//
	// Added this 090401
	//
	p_soldier->Start_Observers();
	CoopInventoryManager::Apply_To_Player(p_soldier);
	if (IS_COOP_MISSION) {
		CoopDebugLog::Log("cGod::Create_Commando observers/inventory started client_id=%d soldier=%p net_id=%d",
			client_id, p_soldier, p_soldier->Get_Network_ID());
	}

	The_Game()->Soldier_Added(p_soldier);
	if (IS_COOP_MISSION) {
		CoopDebugLog::Log("cGod::Create_Commando done client_id=%d soldier=%p net_id=%d",
			client_id, p_soldier, p_soldier->Get_Network_ID());
	}

	if (cNetwork::I_Am_Client() && client_id == cNetwork::Get_My_Id()) {
		ActionParamsStruct parameters;
		WWASSERT(p_soldier->Get_Action() != NULL);
		p_soldier->Get_Action()->Follow_Input(parameters);
		CombatManager::Set_The_Star(p_soldier);

		//
		//	Let the cheat manager apply its cheats to the new player
		//
		CheatMgrClass::Get_Instance()->Apply_Cheats();


#ifdef WWDEBUG
		Reinitialize_Ai_On_Star();
#endif // WWDEBUG
	}

	return p_soldier;
}

//-----------------------------------------------------------------------------
SoldierGameObj * cGod::Create_Commando(cPlayer * p_player, bool prefer_ally_spawn)
{
   WWASSERT(cNetwork::I_Am_Server());
	WWASSERT(p_player != NULL);

	int client_id		= p_player->Get_Id();
	int player_type	= p_player->Get_Player_Type();
	//int model_num		= p_player->Get_Model();

	return Create_Commando(client_id, player_type, prefer_ally_spawn/*, model_num*/);
}

//-----------------------------------------------------------------------------
bool cGod::Set_Coop_Character_Preset(int client_id, const char *preset_name)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server() || preset_name == NULL || preset_name[0] == 0) {
		return false;
	}

	cPlayer *player = cPlayerManager::Find_Player(client_id);
	if (player == NULL || !player->Is_Active()) {
		return false;
	}

	StringClass selected_preset(preset_name, true);
	if (!Is_Valid_Coop_Character_Preset(selected_preset)) {
		Debug_Say(("Co-op character preset %s is invalid\n", selected_preset.Peek_Buffer()));
		return false;
	}

	Store_Coop_Character_Preset(client_id, selected_preset);

	SoldierGameObj *soldier = GameObjManager::Find_Soldier_Of_Client_ID(client_id);
	if (soldier == NULL || soldier->Is_Delete_Pending()) {
		return true;
	}

	return Apply_Coop_Character_Preset(soldier, selected_preset);
}

//-----------------------------------------------------------------------------
bool cGod::Can_Coop_Respawn_Player(int client_id)
{
	if (!IS_COOP_MISSION || !cNetwork::I_Am_Server()) {
		return false;
	}

	cPlayer *player = cPlayerManager::Find_Player(client_id);
	if (player == NULL || !player->Is_Active() || player->Get_Is_In_Game().Is_False()) {
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
SoldierGameObj * cGod::Coop_Respawn_Player(int client_id)
{
	if (!Can_Coop_Respawn_Player(client_id)) {
		return NULL;
	}

	cPlayer *player = cPlayerManager::Find_Player(client_id);
	WWASSERT(player != NULL);
	if (player == NULL) {
		return NULL;
	}

	SoldierGameObj *old_soldier = GameObjManager::Find_Soldier_Of_Client_ID(client_id);
	if (old_soldier != NULL && !old_soldier->Is_Delete_Pending()) {
		old_soldier->Set_Delete_Pending();
	}

	SoldierGameObj *soldier = Create_Commando(player, true);
	if (soldier != NULL) {
		cCoopCameraEvent::Sync_Current_Camera_State();
	}
	return soldier;
}

//-----------------------------------------------------------------------------
void cGod::Create_Grunt(Vector3 & pos)
{
	WWASSERT(cNetwork::I_Am_Server());

	int client_id		= SmartGameObj::SERVER_CONTROL_OWNER;

	WWASSERT(PTheGameData != NULL);
	int player_type	= The_Game()->Choose_Player_Type(NULL, -1, true);
	//int model_num		= rand() % NUM_MP_PLAYABLE_MODELS;

	SoldierGameObj * p_soldier = Create_Commando(
		client_id, player_type/*, model_num*/);
	WWASSERT(p_soldier != NULL);

	p_soldier->Set_Position(pos);
	p_soldier->Perturb_Position();
}

//-----------------------------------------------------------------------------
#ifdef WWDEBUG
void cGod::Reinitialize_Ai_On_Star(void)
{
	WWASSERT(cNetwork::I_Am_Client());

	SmartGameObj * p_my_soldier = GameObjManager::Find_Soldier_Of_Client_ID(cNetwork::Get_My_Id());

	if (p_my_soldier != NULL) {

		//
		// Remove any innate observers
		//
		const GameObjObserverList & observer_list = p_my_soldier->Get_Observers();
		for (int index = 0; index < observer_list.Count(); index++) {
			if (!stricmp(observer_list[index]->Get_Name(), "Innate Soldier")) {
				p_my_soldier->Remove_Observer(observer_list[index]);
				break; // probably not safe to continue
			}
		}

		cPlayer * p_player = cNetwork::Get_My_Player_Object();
		WWASSERT(p_player != NULL);

		ActionParamsStruct parameters;
		WWASSERT(p_my_soldier->Get_Action() != NULL);
		p_my_soldier->Get_Action()->Follow_Input(parameters);

		CombatManager::Set_Is_Star_Determining_Target(true);
	}
}
#endif // WWDEBUG

//-----------------------------------------------------------------------------
Matrix3D		_StarRespawnTM;
InventoryClass	_DeathInventory;

void cGod::Reset( void )
{
	State = GOD_STATE_UNINITIALIZED;
	Reset_Coop_Respawn_Waiting();
	CoopInventoryManager::Reset();
}

void cGod::Exit( void )
{
	State = GOD_STATE_EXITING;
}

void cGod::Star_Killed( void )
{
	if ( State == GOD_STATE_SINGLE_RUNNING ) {
		State = GOD_STATE_SINGLE_DEAD;
		WWDEBUG_SAY(( "Star Killed\n" ));

		_DeathInventory.Store_Inventory( COMBAT_STAR );

		_StarRespawnTM = COMBAT_STAR->Get_Transform();

		DeathOptionsPopupClass * popup = new DeathOptionsPopupClass;
		popup->Start_Dialog();
		popup->Release_Ref();
	} else if ( State == GOD_STATE_MULTIPLAYER || State == GOD_STATE_COOP_RUNNING ) {

		if (GameModeManager::Find ("Combat")->Is_Active ()) {
			if (GameModeManager::Find ("Menu")->Is_Active ()) {
				GameModeManager::Find ("Menu")->Deactivate ();
			} else {
				DialogMgrClass::Flush_Dialogs ();
			}
		}
	}
}

void cGod::Respawn( void )
{
	WWASSERT( State == GOD_STATE_SINGLE_DEAD );
	SoldierGameObj * soldier = Create_Commando( cPlayerManager::Get_Player_Object_List()->Head()->Data() );
	soldier->Set_Transform( _StarRespawnTM );
	_DeathInventory.Restore_Inventory( soldier );

	const char * script_name = CombatManager::Get_Respawn_Script();
	ScriptClass* script = ScriptManager::Create_Script( script_name );
	if (script) {
		soldier->Add_Observer( script );
	}

	State = GOD_STATE_SINGLE_RUNNING;
}

void cGod::Restart( void )
{
	if ( State == GOD_STATE_SINGLE_DEAD ) {
//		WWASSERT( State == GOD_STATE_SINGLE_DEAD );

		State = GOD_STATE_SINGLE_RUNNING;	// Incase we get a second call!
		((CombatGameModeClass *)GameModeManager::Find("Combat"))->Core_Restart();

		//
		//	Reset the player's stats
		//
		cPlayer *player_info = cPlayerManager::Get_Player_Object_List()->Head()->Data();
		if ( player_info != NULL ) {
			player_info->Stats_Reset();
		}

		SoldierGameObj * soldier = Create_Commando( player_info );

		const char * script_name = CombatManager::Get_Start_Script();
		ScriptClass* script = ScriptManager::Create_Script( script_name );
		if (script) {
			soldier->Add_Observer( script );
		}
		State = GOD_STATE_SINGLE_RUNNING;	// restart makes it exit

	}
}

void cGod::Load_Game( void )
{
	WWASSERT( State == GOD_STATE_SINGLE_DEAD );
	GameInitMgrClass::End_Game();
	RenegadeDialogMgrClass::Goto_Location (RenegadeDialogMgrClass::LOC_LOAD_GAME);
}


void cGod::Mission_Failed( void )
{
	if ( State == GOD_STATE_SINGLE_RUNNING ) {
		State = GOD_STATE_SINGLE_DEAD;
		WWDEBUG_SAY(( "Mission Failed\n" ));

		FailedOptionsPopupClass * popup = new FailedOptionsPopupClass;
		popup->Start_Dialog();
		popup->Release_Ref();
	}
}


void cGod::Store_Inventory( SoldierGameObj * soldier )
{
	LevelStartInventory.Store_Inventory( soldier );
	EncyclopediaMgrClass::Store_Data();
	return ;
}

void cGod::Restore_Inventory( SoldierGameObj * soldier )
{
	LevelStartInventory.Restore_Inventory( soldier );
	EncyclopediaMgrClass::Restore_Data();
	return ;
}

void cGod::Reset_Inventory( void )
{
	LevelStartInventory.Reset();
}
