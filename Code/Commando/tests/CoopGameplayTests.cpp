/*
** Copyright 2026 OpenW3D Contributors.
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "always.h"
#include "gdcoopmission.h"
#include "gamemode.h"
#include "gametype.h"
#include "combat.h"
#include "smartgameobj.h"
#include "soldier.h"
#include "vehicle.h"
#include "pscene.h"
#include "humanphys.h"
#include "nullrobj.h"
#include "playertype.h"
#include "damage.h"
#include "chunkio.h"
#include "saveload.h"
#include "ramfile.h"
#include "ffactory.h"
#include "bitstream.h"
#include "bitpackids.h"
#include "wwpacket.h"
#include "null/NullAudio.h"
#include "gameobjmanager.h"
#include "playermanager.h"
#include "player.h"
#include "cnetwork.h"
#include "connect.h"
#include "singlepl.h"
#include "coopcameraevent.h"
#include "coopcamerastate.h"
#include "coopnetupdatetimer.h"
#include "useroptions.h"
#include "consolefunction.h"
#include "registry.h"
#include "coopcontrollockevent.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

void Near(float actual, float expected, const char *message)
{
	if (std::fabs(actual - expected) > 0.002f || !std::isfinite(actual)) {
		std::fprintf(stderr, "FAIL: %s (expected %.3f, got %.3f)\n", message, expected, actual);
		std::exit(1);
	}
}

// Use real armor parsing and damage calculations without installed game assets.
class TestFileFactory final : public FileFactoryClass {
public:
	FileClass *Get_File(const char *filename) override
	{
		static char armor[] =
			"[Armor_Types]\n0=Test\n"
			"[Warhead_Types]\n0=Test\n"
			"[Armor_Save_IDs]\nTest=0\n"
			"[Warhead_Save_IDs]\nTest=0\n"
			"[Scale_Test]\nTest=1\n"
			"[Shield_Test]\nTest=0\n";
		return std::strcmp(filename, "armor.ini") == 0
			? new RAMFileClass(armor, sizeof(armor) - 1) : nullptr;
	}
	void Return_File(FileClass *file) override { delete file; }
};

class InactiveWolMode final : public GameModeClass {
public:
	const char *Name() override { return "WOL"; }
	void Init() override {}
	void Shutdown() override {}
	void Render() override {}
	void Think() override {}
};

class TestSaveLoadSystem : public SaveLoadSystemClass {
public:
	static void Finish()
	{
		PointerRemapper.Process();
		ResetPointerRemapper();
		Post_Load_Processing(nullptr);
	}
};

// Leave physics uninitialized; exercise the real damageable load callback.
class TestSmartGameObj final : public SmartGameObj {
public:
	void Init() override {}
	const PersistFactoryClass &Get_Factory() const override { std::abort(); }
	void On_Post_Load() override { DamageableGameObj::On_Post_Load(); }
};

// Restore the physics reference through the same public load/remap interface
// used by saved levels. A null model provides transforms without a graphics device.
void Attach_Test_Physics(PhysicalGameObj &object, PhysClass *physics)
{
	Null3DObjClass *model = new Null3DObjClass;
	physics->PhysClass::Set_Model(model);
	model->Release_Ref();
	char bytes[128] = {};
	RAMFileClass file(bytes, sizeof(bytes));
	file.Open(FileClass::WRITE);
	ChunkSaveClass save(&file);
	save.Begin_Chunk(910991146);
	CombatPhysObserverClass *observer = &object;
	WRITE_MICRO_CHUNK_PTR(save, 3, observer);
	WRITE_MICRO_CHUNK_PTR(save, 10, physics);
	save.End_Chunk();
	file.Close();
	file.Open(FileClass::READ);
	ChunkLoadClass load(&file);
	object.PhysicalGameObj::Load(load);
	void *saved_id = reinterpret_cast<void *>(
		static_cast<uintptr_t>(SaveLoadSystemClass::Serialize_Pointer(physics)));
	SaveLoadSystemClass::Register_Pointer(saved_id, physics);
	TestSaveLoadSystem::Finish();
	Check(object.Peek_Physical_Object() == physics, "restore physics fixture reference");
	physics->Release_Ref();
}

class TestSoldierGameObj final : public SoldierGameObj {
public:
	TestSoldierGameObj() { Attach_Test_Physics(*this, new HumanPhysClass); }
	void On_Post_Load() override { DamageableGameObj::On_Post_Load(); }
};

class TestVehicleGameObj final : public VehicleGameObj {
public:
	TestVehicleGameObj() { Attach_Test_Physics(*this, new HumanPhysClass); }
	void On_Post_Load() override { DamageableGameObj::On_Post_Load(); }
	void Set_Test_Driver(SoldierGameObj *driver)
	{
		SeatOccupants.Resize(2);
		SeatOccupants[0] = driver;
		SeatOccupants[1] = nullptr;
		OccupiedSeats = driver == nullptr ? 0 : 1;
	}
	~TestVehicleGameObj() override { Set_Test_Driver(nullptr); }
};

void Check_Network_Update_Rate()
{
	const GameTypeEnum saved_type = cGameType::Get_Game_Type();
	const int saved_coop_rate = cUserOptions::CoopNetUpdateRate.Get();
	const int saved_rate = cUserOptions::NetUpdateRate.Get();
	RegistryClass::Set_Read_Only(true);
	cUserOptions::NetUpdateRate.Set(10);
	cUserOptions::CoopNetUpdateRate.Set(cUserOptions::DEFAULT_COOP_NET_UPDATE_RATE);
	cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
	Check(cUserOptions::Get_Net_Update_Rate() == 60, "co-op defaults to 60 independently of old multiplayer preference");
	ConsoleFunctionManager::Init();
	ConsoleFunctionManager::Parse_Input("nur 120");
	Check(cUserOptions::Get_Net_Update_Rate() == 120, "nur alias changes effective co-op scheduler/budget rate");
	ConsoleFunctionManager::Parse_Input("net_update_rate 60");
	Check(cUserOptions::Get_Net_Update_Rate() == 60, "full command changes effective co-op rate");
	for (const char *command : {"nur", "nur 0", "nur -1", "nur 4", "nur 121", "nur 999999999999999999999", "nur 60garbage", "nur text"}) {
		ConsoleFunctionManager::Parse_Input(command);
		Check(cUserOptions::Get_Net_Update_Rate() == 60, "query and invalid input preserve rate");
	}
	ConsoleFunctionManager::Parse_Input("nur 5");
	Check(cUserOptions::Get_Net_Update_Rate() == 5, "co-op honors reductions below original hardcoded 30");
	cUserOptions::CoopNetUpdateRate.Set(0);
	Check(cUserOptions::Get_Net_Update_Rate() == 5, "invalid saved zero cannot divide by zero in budget/scheduler");
	cUserOptions::CoopNetUpdateRate.Set(999);
	Check(cUserOptions::Get_Net_Update_Rate() == 120, "invalid saved high value is bounded");
	ConsoleFunctionManager::Parse_Input("nur 90");
	cGameType::Set_Game_Type(GAMETYPE_MULTIPLAY);
	Check(cUserOptions::Get_Net_Update_Rate() == 10, "co-op command preserves regular multiplayer preference");
	ConsoleFunctionManager::Parse_Input("nur 60");
	Check(cUserOptions::Get_Net_Update_Rate() == 10, "regular multiplayer retains 30 Hz maximum");
	ConsoleFunctionManager::Parse_Input("nur 30");
	Check(cUserOptions::Get_Net_Update_Rate() == 30, "regular multiplayer command still works");
	cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
	Check(cUserOptions::Get_Net_Update_Rate() == 90, "co-op preference survives leaving and reentering mode");
	ConsoleFunctionManager::Shutdown();
	cUserOptions::NetUpdateRate.Set(saved_rate);
	cUserOptions::CoopNetUpdateRate.Set(saved_coop_rate);
	RegistryClass::Set_Read_Only(false);
	cGameType::Set_Game_Type(saved_type);

	// Simulate integer-ms timestamps produced by common render rates for 10s.
	// Check actual sends, including when rendering cannot keep up with nur.
	for (int fps : {30, 60, 120, 144, 240}) {
		for (int rate : {5, 30, 60, 90, 120}) {
			CoopNetUpdateTimer timer;
			int sends = 0;
			for (int frame = 0; frame <= fps * 10; ++frame) {
				const uint32_t now = static_cast<uint32_t>((uint64_t(frame) * 1000) / fps);
				if (timer.Is_Due(now, rate)) ++sends;
				Check(!timer.Is_Due(now, rate), "timer never sends twice on the same timestamp");
			}
			const int expected = (fps < rate ? fps : rate) * 10 + 1;
			if (std::abs(sends - expected) > 1) {
				std::fprintf(stderr, "FAIL: %d FPS / %d Hz produced %d sends, expected %d\n", fps, rate, sends, expected);
				std::exit(1);
			}
		}
	}
	CoopNetUpdateTimer timer;
	Check(timer.Is_Due(0xfffffff0u, 60), "first update is immediate");
	Check(timer.Is_Due(1, 60), "timer handles millisecond clock wrap");
	Check(!timer.Is_Due(2, 60), "normal rate limit follows wrapped clock");
	Check(timer.Is_Due(2, 60, true), "urgent load/disconnect flush bypasses limit");
	Check(!timer.Is_Due(3, 60), "urgent send resets accumulated credit");
	Check(timer.Is_Due(3, 120), "runtime rate change applies immediately");
	Check(timer.Is_Due(10003, 120), "first frame after stall sends once");
	Check(!timer.Is_Due(10003, 120), "stall does not cause catch-up burst");
	Check(!timer.Is_Due(10004, 0), "invalid rate is safe");
}
void Check_Settings_Isolation()
{
	cGameDataCoopMission active;
	PTheGameData = &active;
	active.Set_Enemy_Health_Multiplier(2.5f);
	active.Set_Enemy_Damage_Multiplier(0.5f);
	active.Set_AI_Hearing_Multiplier(3.0f);
	active.Set_Sprint_Enabled(false);
	active.Set_Difficulty_Level(2);

	{
		// This constructor runs on every incoming co-op LAN advertisement.
		cGameDataCoopMission advertised;
		Near(cGameType::Get_Coop_Enemy_Health_Multiplier(), 2.5f, "LAN description preserves health setting");
		Near(cGameType::Get_Coop_Enemy_Damage_Multiplier(), 0.5f, "LAN description preserves damage setting");
		Check(!cGameType::Is_Coop_Sprint_Enabled(), "LAN description preserves sprint setting");
		advertised.Set_Enemy_Health_Multiplier(4.0f);
		advertised.Set_Enemy_Damage_Multiplier(4.0f);
		advertised.Set_AI_Hearing_Multiplier(1.0f);
		advertised.Set_Sprint_Enabled(true);
		advertised.Set_Difficulty_Level(0);
		advertised.Apply_Global_Settings();
		Near(cGameType::Get_Coop_Enemy_Health_Multiplier(), 2.5f, "inactive setters preserve active health");
		Near(cGameType::Get_Coop_AI_Hearing_Multiplier(), 3.0f, "inactive setters preserve active hearing");
		Check(CombatManager::Get_Difficulty_Level() == 2, "inactive settings preserve active difficulty");

		PTheGameData = &advertised;
		advertised.Apply_Global_Settings();
		Near(cGameType::Get_Coop_Enemy_Health_Multiplier(), 4.0f, "selected game applies its settings");
		cPacket settings;
		active.Export_Tier_2_Data(settings);
		advertised.Import_Tier_2_Data(settings);
		Near(cGameType::Get_Coop_Enemy_Health_Multiplier(), 2.5f, "network import applies health setting");
		Near(cGameType::Get_Coop_Enemy_Damage_Multiplier(), 0.5f, "network import applies damage setting");
		Check(!cGameType::Is_Coop_Sprint_Enabled(), "network import applies sprint setting");
	}
	PTheGameData = nullptr;
}

template<class T>
void Write_Micro(ChunkSaveClass &save, unsigned id, const T &value)
{
	save.Begin_Micro_Chunk(id);
	save.Write(&value, sizeof(value));
	save.End_Micro_Chunk();
}

void Load_Defense_Fixture(TestSmartGameObj &object, int team, float health_max,
	float health, float armor_max, float armor, float applied_multiplier = 0.0f)
{
	// Legacy DamageableGameObj chunk IDs. Original mission objects lack
	// microchunk 3; co-op saves include it to prevent repeated scaling.
	char bytes[4096] = {};
	RAMFileClass file(bytes, sizeof(bytes));
	Check(file.Open(FileClass::WRITE) != 0, "open memory save");
	ChunkSaveClass save(&file);
	save.Begin_Chunk(207011214);
	Write_Micro(save, 1, team);
	if (applied_multiplier > 0.0f) {
		Write_Micro(save, 3, applied_multiplier);
	}
	save.End_Chunk();
	DefenseObjectDefClass definition;
	DefenseObjectClass defense;
	defense.Init(definition, nullptr);
	defense.Set_Health_Max(health_max);
	defense.Set_Health(health);
	defense.Set_Shield_Strength_Max(armor_max);
	defense.Set_Shield_Strength(armor);
	save.Begin_Chunk(207011213);
	defense.Save(save);
	save.End_Chunk();
	file.Close();
	Check(file.Open(FileClass::READ) != 0, "reopen memory save");
	ChunkLoadClass load(&file);
	Check(object.DamageableGameObj::Load(load), "load level defense fixture");
	TestSaveLoadSystem::Finish();
	object.On_Post_Load();
}

void Check_Enemy_Health()
{
	cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
	cGameType::Set_Coop_Enemy_Health_Multiplier(2.0f);
	CombatManager::Set_I_Am_Server(true);
	TestSmartGameObj enemy;
	Load_Defense_Fixture(enemy, PLAYERTYPE_NOD, 100, 75, 50, 20);
	TestSmartGameObj spawned_enemy;
	spawned_enemy.Get_Defense_Object()->Set_Health_Max(100);
	spawned_enemy.Get_Defense_Object()->Set_Health(100);
	spawned_enemy.Set_Player_Type(PLAYERTYPE_NOD);
	Near(spawned_enemy.Get_Defense_Object()->Get_Health_Max(), 200, "spawned enemy health scales");
	auto *defense = enemy.Get_Defense_Object();
	Near(defense->Get_Health_Max(), 200, "placed enemy maximum health scales");
	Near(defense->Get_Health(), 150, "placed enemy current health scales");
	Near(defense->Get_Shield_Strength_Max(), 100, "placed enemy maximum armor scales");
	Near(defense->Get_Shield_Strength(), 40, "placed enemy current armor scales");
	enemy.On_Post_Load();
	enemy.Set_Player_Type(PLAYERTYPE_NOD);
	Near(defense->Get_Health_Max(), 200, "repeated post-load/team update is idempotent");

	TestSmartGameObj saved_enemy;
	Load_Defense_Fixture(saved_enemy, PLAYERTYPE_NOD, 200, 150, 100, 40, 2.0f);
	Near(saved_enemy.Get_Defense_Object()->Get_Health(), 150, "saved co-op health is not multiplied again");
	cGameType::Set_Coop_Enemy_Health_Multiplier(3.0f);
	saved_enemy.On_Post_Load();
	Near(saved_enemy.Get_Defense_Object()->Get_Health_Max(), 300, "saved health rescales to a changed setting");
	Near(saved_enemy.Get_Defense_Object()->Get_Health(), 225, "rescaling preserves wounds");

	TestSmartGameObj ally;
	Load_Defense_Fixture(ally, PLAYERTYPE_GDI, 100, 75, 50, 20);
	Near(ally.Get_Defense_Object()->Get_Health_Max(), 100, "allied health stays unchanged");
	CombatManager::Set_I_Am_Server(false);
	enemy.Set_Player_Type(PLAYERTYPE_NOD);
	Near(defense->Get_Health_Max(), 200, "client team updates cannot rescale authoritative health");

	cGameType::Set_Game_Type(GAMETYPE_MISSION);
	CombatManager::Set_I_Am_Server(true);
	TestSmartGameObj solo_enemy;
	Load_Defense_Fixture(solo_enemy, PLAYERTYPE_NOD, 100, 75, 50, 20);
	Near(solo_enemy.Get_Defense_Object()->Get_Health_Max(), 100, "single-player health stays unchanged");
}

void Check_Defense_Replication()
{
	cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
	DefenseObjectClass server, client;
	server.Set_Health_Max(350);
	server.Set_Health(275);
	server.Set_Shield_Strength_Max(180);
	server.Set_Shield_Strength(90);
	BitStreamClass packet;
	server.Export(packet);
	client.Import(packet);
	Near(client.Get_Health_Max(), 350, "client receives enemy maximum health");
	Near(client.Get_Health(), 275, "client receives current health");
	Near(client.Get_Shield_Strength_Max(), 180, "client receives enemy maximum armor");
	Near(client.Get_Shield_Strength(), 90, "client receives current armor");

	cGameType::Set_Game_Type(GAMETYPE_MULTIPLAY);
	BitStreamClass legacy;
	server.Export(legacy);
	bool zero;
	int health, armor;
	unsigned shield_type;
	legacy.Get(zero);
	legacy.Get(health, BITPACK_HEALTH);
	legacy.Get(armor, BITPACK_SHIELD_STRENGTH);
	legacy.Get(shield_type, BITPACK_SHIELD_TYPE);
	Check(!zero && health == 275 && armor == 90 && shield_type == 0, "multiplayer defense wire format is unchanged");
}

void Reset_Health(DamageableGameObj &object)
{
	auto *defense = object.Get_Defense_Object();
	defense->Set_Owner(&object);
	defense->Set_Health_Max(1000);
	defense->Set_Health(1000);
	defense->Set_Shield_Strength_Max(0);
	defense->Set_Shield_Strength(0);
}

void Check_Enemy_Damage()
{
	PhysicsSceneClass scene;
	cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
	cGameType::Set_Coop_Enemy_Health_Multiplier(1.0f);
	cGameType::Set_Coop_Enemy_Damage_Multiplier(3.0f);
	CombatManager::Set_I_Am_Server(true);
	CombatManager::Set_Difficulty_Level(2);
	TestSmartGameObj enemy;
	enemy.Set_Player_Type(PLAYERTYPE_NOD);
	TestSoldierGameObj player;
	player.Set_Control_Owner(2);
	player.Set_Player_Type(PLAYERTYPE_GDI);
	Reset_Health(player);
	OffenseObjectClass hit(10, 0, &enemy);
	player.Get_Defense_Object()->Do_Damage(hit);
	Near(player.Get_Defense_Object()->Get_Health(), 970, "enemy damage scales against a guest");

	TestVehicleGameObj vehicle;
	vehicle.Set_Test_Driver(&player);
	Reset_Health(vehicle);
	vehicle.Get_Defense_Object()->Do_Damage(hit);
	Near(vehicle.Get_Defense_Object()->Get_Health(), 970, "enemy damage scales against a guest's vehicle");
	cGameType::Set_Coop_Enemy_Damage_Multiplier(0.0f);
	vehicle.Get_Defense_Object()->Do_Damage(hit);
	Near(vehicle.Get_Defense_Object()->Get_Health(), 970, "zero enemy damage makes driven vehicle immune");
	OffenseObjectClass repair(-10, 0, &enemy);
	vehicle.Get_Defense_Object()->Do_Damage(repair);
	Near(vehicle.Get_Defense_Object()->Get_Health(), 980, "enemy damage setting does not disable repairs");

	CombatManager::Set_Difficulty_Level(0);
	Reset_Health(enemy);
	OffenseObjectClass player_hit(10, 0, &player);
	enemy.Get_Defense_Object()->Do_Damage(player_hit);
	Near(enemy.Get_Defense_Object()->Get_Health(), 980, "guest receives easy-difficulty outgoing damage bonus");
	cGameType::Set_Game_Type(GAMETYPE_MULTIPLAY);
	Reset_Health(player);
	player.Get_Defense_Object()->Do_Damage(hit);
	Near(player.Get_Defense_Object()->Get_Health(), 990, "co-op damage setting does not affect multiplayer");
	CombatManager::Set_I_Am_Server(false);
}

void Check_Multiplayer_Teammates_And_Readiness()
{
    cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
    CombatManager::Set_I_Am_Server(false);
    TestSoldierGameObj soldiers[4];
    DynamicVectorClass<SoldierGameObj *> teammates;
    for (int i = 0; i < 4; ++i) {
        soldiers[i].Set_Control_Owner(i + 1);
        soldiers[i].Set_Player_Type(PLAYERTYPE_GDI);
        Reset_Health(soldiers[i]);
    }
    for (int i = 0; i < 4; ++i) {
        GameObjManager::Get_Living_Teammates(&soldiers[i], teammates);
        Check(teammates.Count() == 3, "each of four players sees all three teammates");
        for (int j = 0; j < teammates.Count(); ++j) {
            Check(teammates[j] != &soldiers[i], "teammate markers exclude the viewer");
        }
    }
    soldiers[3].Set_Control_Owner(-1);
    for (int i = 0; i < 3; ++i) {
        GameObjManager::Get_Living_Teammates(&soldiers[i], teammates);
        Check(teammates.Count() == 2, "each of three players sees both teammates");
    }
    soldiers[1].Get_Defense_Object()->Set_Health(0);
    GameObjManager::Get_Living_Teammates(&soldiers[0], teammates);
    Check(teammates.Count() == 1 && teammates[0] == &soldiers[2], "dead teammates do not hide later players");
    soldiers[2].Set_Player_Type(PLAYERTYPE_NOD);
    GameObjManager::Get_Living_Teammates(&soldiers[0], teammates);
    Check(teammates.Count() == 0, "enemy players are not teammate markers");

    Check(!cPlayerManager::All_Active_Players_In_Game(), "empty lobby cannot start a mission");
    cPlayer *players[4] = { new cPlayer, new cPlayer, new cPlayer, new cPlayer };
    for (int i = 0; i < 4; ++i) {
        players[i]->Set_Id(i + 1);
        players[i]->Set_Is_Active(true);
        players[i]->Set_Is_In_Game(false);
    }
    players[0]->Set_Is_In_Game(true);
    players[1]->Set_Is_In_Game(true);
    Check(!cPlayerManager::All_Active_Players_In_Game(), "host and one guest cannot start ahead of other loaders");
    players[2]->Set_Is_In_Game(true);
    Check(!cPlayerManager::All_Active_Players_In_Game(), "a fourth loader holds the readiness barrier");
    players[3]->Set_Is_Active(false);
    Check(cPlayerManager::All_Active_Players_In_Game(), "a disconnected fourth player does not block the three-player start");
    players[1]->Set_Is_In_Game(false);
    Check(!cPlayerManager::All_Active_Players_In_Game(), "next map waits for each loading acknowledgement again");
    players[1]->Set_Is_In_Game(true);
    Check(cPlayerManager::All_Active_Players_In_Game(), "all three loading acknowledgements release the barrier");
    for (auto *player : players) { static_cast<NetworkObjectClass *>(player)->Delete(); }
}

struct CameraSnapshot {
    int HostId;
    bool HasLook;
    Vector3 Look;
};
CameraSnapshot Read_Camera_Snapshot(BitStreamClass &packet)
{
    char map[256];
    unsigned int sequence;
    CameraSnapshot snapshot;
    packet.Get_Terminated_String(map, sizeof(map), true);
    packet.Get(sequence);
    packet.Get(snapshot.HasLook);
    packet.Get(snapshot.HostId);
    packet.Get(snapshot.Look.X);
    packet.Get(snapshot.Look.Y);
    packet.Get(snapshot.Look.Z);
    return snapshot;
}

CameraSnapshot Read_Targeted_Camera_Snapshot(int client_id)
{
    int count = NetworkObjectMgrClass::Get_Object_Count();
    cCoopCameraEvent::Sync_Current_Camera_State(client_id);
    Check(NetworkObjectMgrClass::Get_Object_Count() == count + 1, "camera synchronization creates one complete snapshot");
    cCoopCameraEvent *event = nullptr;
    for (int i = 0; i < NetworkObjectMgrClass::Get_Object_Count(); ++i) {
        auto *object = NetworkObjectMgrClass::Get_Object(i);
        if (object->Get_Network_Class_ID() == NETCLASSID_COOPCAMERAEVENT &&
            (event == nullptr || event->Get_Network_ID() < object->Get_Network_ID())) {
            event = static_cast<cCoopCameraEvent *>(object);
        }
    }
    Check(event != nullptr, "targeted camera event exists");
    for (int peer = 1; peer <= 4; ++peer) {
        Check(event->Get_Object_Dirty_Bit(peer, NetworkObjectClass::BIT_CREATION) == (peer == client_id),
            "a respawn snapshot is sent only to its intended player");
    }
    BitStreamClass packet;
    event->Export_Creation(packet);
    CameraSnapshot snapshot = Read_Camera_Snapshot(packet);
    delete event;
    return snapshot;
}

void Check_Cinematic_Replication()
{
    CoopCameraState pending;
    pending.Set_Host(123);
    pending.Set_Look(Vector3(4, 5, 6));
    Check(pending.HostPending && pending.LookPending, "camera and look remain pending until the model is ready");
    pending.Set_Host(0);
    Check(pending.HostPending && pending.HostId == 0 && !pending.LookPending && !pending.HasLook,
        "release cancels a missing camera and its pending look");
    pending.Set_Host(456);
    pending.Host_Was_Applied();
    Check(pending.HostApplied && !pending.HostPending, "applied host is tracked for deletion handling");
    pending.Reset();
    Check(!pending.HostApplied && !pending.HostPending && pending.HostId == 0,
        "map reset cannot replay an old cinematic host");

    cGameType::Set_Game_Type(GAMETYPE_COOP_MISSION);
    cGameDataCoopMission game;
    PTheGameData = &game;
    StringClass first_map("M13.mix");
    game.Set_Map_Name(first_map);
    cCoopCameraEvent::Reset(true);
    cCoopControlLockEvent::Reset(true);
    // The engine's in-memory transport avoids binding sockets or contacting
    // peers; event serialization/import and per-client dirty bits remain real.
    cSinglePlayerData::Init();
    {
        cConnection connection;
        connection.Init_As_Client(0UL, 0);
        auto server = [&]() {
            cNetwork::PClientConnection = nullptr;
            cNetwork::PServerConnection = &connection;
            NetworkObjectClass::Set_Is_Server(true);
        };
        auto client = [&]() {
            cNetwork::PServerConnection = nullptr;
            cNetwork::PClientConnection = &connection;
            NetworkObjectClass::Set_Is_Server(false);
        };
        server();
        TestSoldierGameObj host_model;
        int host_id = host_model.Get_ID();
        cCoopCameraEvent host, look, release;
        host.Init_Camera_Host(&host_model);
        look.Init_Force_Look(Vector3(10, 20, 30));
        BitStreamClass host_packet, look_packet, release_packet;
        host.Export_Creation(host_packet);
        look.Export_Creation(look_packet);
        for (int peer = 2; peer <= 4; ++peer) {
            CameraSnapshot snapshot = Read_Targeted_Camera_Snapshot(peer);
            Check(snapshot.HostId == host_id && snapshot.HasLook, "all guests receive the camera host and look together");
            Near(snapshot.Look.Y, 20, "camera look target survives serialization");
        }
        release.Init_Camera_Host(nullptr);
        release.Export_Creation(release_packet);

        cCoopCameraEvent::Reset();
        client();
        cCoopCameraEvent received;
        received.Import_Creation(look_packet); // Deliberately arrive before host/model.
        server();
        CameraSnapshot snapshot = Read_Targeted_Camera_Snapshot(3);
        Check(snapshot.HostId == host_id && snapshot.HasLook, "missing local camera retains the full desired snapshot");
        client();
        received.Import_Creation(release_packet);
        received.Import_Creation(host_packet); // A late older host must not relock.
        server();
        snapshot = Read_Targeted_Camera_Snapshot(3);
        Check(snapshot.HostId == 0 && !snapshot.HasLook, "late camera packets cannot override a release");

        cCoopControlLockEvent lock, unlock;
        lock.Init(false);
        unlock.Init(true);
        BitStreamClass lock_packet, unlock_packet;
        lock.Export_Creation(lock_packet);
        unlock.Export_Creation(unlock_packet);
        cCoopControlLockEvent::Reset();
        client();
        cCoopControlLockEvent received_control;
        received_control.Import_Creation(lock_packet);
        TestSoldierGameObj new_body;
        cCoopControlLockEvent::Apply_Current_Control_State(&new_body);
        Check(!new_body.Is_Control_Enabled(), "control lock survives arrival before the player's body");
        received_control.Import_Creation(unlock_packet);
        cCoopControlLockEvent::Apply_Current_Control_State(&new_body);
        Check(new_body.Is_Control_Enabled(), "unlock restores a subsequently created body");

        server();
        cCoopCameraEvent stale_camera;
        stale_camera.Init_Camera_Host(&host_model);
        cCoopControlLockEvent stale_lock;
        stale_lock.Init(false);
        BitStreamClass stale_camera_packet, stale_lock_packet;
        stale_camera.Export_Creation(stale_camera_packet);
        stale_lock.Export_Creation(stale_lock_packet);
        StringClass next_map("M01.mix");
        game.Set_Map_Name(next_map);
        cCoopCameraEvent::Reset();
        cCoopControlLockEvent::Reset();
        client();
        received.Import_Creation(stale_camera_packet);
        received_control.Import_Creation(stale_lock_packet);
        cCoopControlLockEvent::Apply_Current_Control_State(&new_body);
        Check(new_body.Is_Control_Enabled(), "previous map's lock cannot freeze the new mission");
        server();
        snapshot = Read_Targeted_Camera_Snapshot(2);
        Check(snapshot.HostId == 0 && !snapshot.HasLook, "M13 camera state cannot leak into M01");

        cCoopCameraEvent::Reset(true);
        cCoopControlLockEvent::Reset(true);
        cNetwork::PServerConnection = nullptr;
        cNetwork::PClientConnection = nullptr;
        NetworkObjectClass::Set_Is_Server(false);
    }
    cSinglePlayerData::Cleanup();
    PTheGameData = nullptr;
}
} // namespace

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
	std::puts("Initializing null audio...");
	NullAudioClass audio(true);
	TestFileFactory files;
	FileFactoryClass *previous_files = _TheFileFactory;
	_TheFileFactory = &files;
	std::puts("Initializing armor data...");
	ArmorWarheadManager::Init();
	DefenseObjectClass::Set_Precision();
	InactiveWolMode wol;
	GameModeManager::Add(&wol);

	std::puts("Checking nur command, mode isolation and 5-120 Hz pacing...");
	Check_Network_Update_Rate();
	std::puts("Checking settings isolation and network import...");
	Check_Settings_Isolation();
	std::puts("Checking loaded enemy health and save compatibility...");
	Check_Enemy_Health();
	std::puts("Checking defense replication...");
	Check_Defense_Replication();
	std::puts("Checking infantry/vehicle damage and guest difficulty...");
	Check_Enemy_Damage();

    std::puts("Checking three/four-player markers and loading readiness...");
    Check_Multiplayer_Teammates_And_Readiness();
    std::puts("Checking cinematic snapshots, delayed objects, release and map resets...");
    Check_Cinematic_Replication();

    GameModeManager::Remove(&wol);
	ArmorWarheadManager::Shutdown();
	_TheFileFactory = previous_files;
	std::puts("All co-op gameplay regression checks passed.");
	return 0;
}
