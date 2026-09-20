# Campaign co-op branch review

Reviewed **feature/campaign-coop** after the upstream merge at **bb4a252c**.
Priority: unreliable gameplay settings, especially enemy multipliers. Cinematic
findings are recorded for a focused multiplayer follow-up.

## Fixes from this review

| Priority | Finding | Result |
| --- | --- | --- |
| P1 | Constructing a temporary co-op game description applied defaults to global gameplay state. The LAN advertisement handler does this while receiving other servers' announcements. | Constructors leave globals alone. Setters and explicit application update globals only for the selected session. This protects enemy multipliers, sprint, pickups, difficulty, and AI settings together. |
| P1 | Mission-placed enemies load their team and defense directly, bypassing the health multiplier's only hook in Set_Player_Type. | The damageable post-load callback applies the multiplier on the server. Existing saved multiplier metadata prevents applying it twice. |
| P2 | Defense replication sent current health/armor, but omitted their scaled maximum values. Clients could use preset limits and rescale received values during team updates. | Co-op packets carry both maximum values. The server owns health scaling; clients consume its values. |
| P2 | Enemy damage scaling tested only human infantry, so entering a vehicle bypassed the setting. | The target check also recognizes vehicles driven or fired by a human. Repairs and standard multiplayer retain their existing behavior. |
| P2 | Campaign difficulty's outgoing damage bonus used the server's COMBAT_STAR, excluding co-op guests. | Every human-controlled co-op soldier receives the same difficulty bonus. |
| P2 | AI weapon-error scaling affected only the action's error contribution. The preset weapon-error contribution bypassed the setting. | The multiplier applies to the final error angle, after selecting the action override when present. |

Main implementation locations:

- [Session settings](Code/Commando/gdcoopmission.cpp): constructor, setters, Apply_Global_Settings.
- [Enemy health](Code/Combat/damageablegameobj.cpp): On_Post_Load, Apply_Coop_Enemy_Health_Multiplier.
- [Damage and defense replication](Code/Combat/damage.cpp): Do_Damage, Import, Export.
- [AI accuracy](Code/Combat/action.cpp): Attack_Absolute.

## Cinematic findings requiring follow-up

### P1: One player's cinematic can freeze everyone outside the intended area

[The control callback](Code/Commando/coopcontrollockevent.cpp) broadcasts a
script's player control change to every active player, regardless of location.
The mission script still moves and hides only the player resolved by STAR.

A concrete example is M11_Start_FirstMidtro_Zone_JDG in
[Mission11.cpp](Code/Scripts/Mission11.cpp), around lines 6540-6680. The Petrova
midtro disables control, hides and repositions STAR, then closes arena doors.
The co-op callback locks all players; it does not move the others inside.
On completion, the script again resolves STAR for repositioning and visibility.
Because STAR chooses the nearest player on each evaluation, moving that player
can even change which player receives the next command.

This is a code-level explanation consistent with the reported wrong-area lock.
The exact playthrough incident has not been reproduced.

A fix should retain the participating players for the duration of the sequence,
place them safely inside the arena, and restore their individual control and
visibility on completion, abort, death, and disconnect.

### P1: Camera events can arrive before the camera object exists

[cCoopCameraEvent::Act](Code/Commando/coopcameraevent.cpp) looks up the host
object once. A missing object or combat camera causes the operation to do
nothing. Import_Creation then deletes the event, so the requested camera is
never applied when the model arrives later.

The follow-up needs pending camera state that resolves after object creation,
with ordering so an old pending camera cannot override a newer release event.

### P2: Cached cinematic state survives mission/session boundaries

CurrentCoopCameraHostId and the force-look state in
[coopcameraevent.cpp](Code/Commando/coopcameraevent.cpp) have no level/session
reset. [god.cpp](Code/Commando/god.cpp) rebroadcasts this cached state to all
clients when a player receives a body or respawns.

A later map can therefore replay an old object ID or force-look target. Camera
state needs a level/session reset and synchronization targeted at the joining or
respawning client. Control locks also lack equivalent persistent state for a
player whose soldier did not exist when the event arrived.

## Verification

The opt-in W3D_BUILD_COOP_TESTS CMake option adds
[CoopGameplayTests.cpp](Code/Commando/tests/CoopGameplayTests.cpp). It links the
actual game code and uses memory-backed armor/level fixtures without game assets.

Coverage includes temporary game-description isolation, selecting a session,
network settings import, original mission health records, spawned enemies, saved scaled health,
repeat load/team notifications, changed multiplier values, allied and
single-player exclusions, server authority, co-op defense replication, legacy
multiplayer packet layout, guest infantry/vehicle damage, zero damage, repairs,
and guest difficulty bonuses.

Validation completed on Windows x64, Release:

- Client (renegade), dedicated server (renegadeserver), and scripts: build passed.
- coop_gameplay_tests: passed.
- wwaudio_lifetime_tests: passed.
- Final CTest run: 2/2 passed.
- git diff --check: passed.

Configure with -DW3D_BUILD_COOP_TESTS=ON, build the coop_gameplay_tests target,
then run CTest with -C Release -R coop_gameplay_tests --output-on-failure.
The accuracy adjustment was checked in source and by compilation; its visual
effect still belongs in the multiplayer playtest.

## Multiplayer playtest cases

Use four players for the main pass, including guests joining and respawning.
The default session size is four; the lobby currently tracks up to sixteen.

1. Start a fresh mission at 200% enemy health and 50% enemy damage. Compare a
   placed enemy and a newly spawned enemy, checking host and guest HUD values.
2. Keep another co-op host advertising on LAN. Confirm settings stay applied
   while its server appears and refreshes.
3. Repeat enemy attacks against both players on foot and in a driven vehicle.
   Repeat with enemy damage at 0%. Check repairs still work.
4. Save and reload wounded enemies. Check health is neither multiplied twice nor
   restored to full health.
5. Start M11's Petrova midtro with one player inside the trigger and another
   outside the arena. Swap who triggers it. Check each player's position,
   visibility, controls, and camera after both midtros.
6. Join or respawn during a cinematic, then transition to another mission and
   respawn again. Check for a missing camera, stale camera attachment, or lock.

All co-op participants need matching updated client/server builds because the
co-op defense packet now includes maximum health and armor. The existing engine
ceiling of 10,000 for each remains in place. Scripts that assign explicit defense
values during boss phases also need gameplay checks.

No two-player campaign or graphical cinematic playtest has been performed as
part of this review.
