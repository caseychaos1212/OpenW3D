# Campaign Co-op Script Audit

Input: `Single Player Level Source Files_script_usage.csv`

## Scope

- 3,365 script attachments across the single-player level sources.
- 985 unique script names.
- The CSV maps most attachments back to `Code/Scripts/...` declaration locations.

## High-Level Findings

- No shipped campaign-attached script in the CSV directly calls `Commands->Get_The_Star()`.
- No shipped campaign-attached script in the CSV directly references `COMBAT_STAR`.
- `Commands->Is_A_Star(...)` is already used by 145 attached scripts, with 148 total hits.
- `Commands->Get_A_Star(...)` is used by 12 attached scripts, with 16 total hits.
- The `STAR` macro is used by 383 attached scripts, with 785 total hits. `STAR` expands to nearest human player at the script owner position, so it is co-op-compatible for rough targeting but risky in event callbacks where the triggering object is already known.

## Changes Applied In This Branch

- Converted callback actor comparisons against `STAR` to `Commands->Is_A_Star(...)` in `Code/Scripts/*.cpp`:
  - `enterer`
  - `poker`
  - `damager`
  - `killer`
- Patched obvious fixed-origin conversation/follow cases:
  - `M05_Activate_Objective_502` now joins the triggering `enterer`.
  - `M05_Activate_Objective_510` now joins the triggering `enterer`.
  - `M05_DEAD6_Engineer` now joins the triggering `poker`.
  - `M01_HON_Dorm_Crapper_JDG` now follows the nearest player to the NPC instead of the nearest player to world origin.

## Main Co-op Risk

The largest script issue is not direct global-star usage. It is callback code comparing event actors to `STAR`:

- `enterer == STAR` / `enterer != STAR`: 164 scripts, 165 hits.
- `poker == STAR`: 13 scripts, 13 hits.
- `damager == STAR`: 41 scripts, 49 hits.
- `killer == STAR`: 3 scripts, 3 hits.

These should be converted to `Commands->Is_A_Star(...)` where the script intent is "a player did this". In co-op, `STAR` can resolve to the other player if that player is closer to the script owner, which can cause Player 2 zone entries, pokes, damage, or kills to be ignored.

## Highest-Impact Files For Callback Rewrite

- `Code/Scripts/Mission01.cpp`: 99 risky callback comparisons.
- `Code/Scripts/Mission11.cpp`: 54 risky callback comparisons.
- `Code/Scripts/Mission04.cpp`: 48 risky callback comparisons.
- `Code/Scripts/Mission09.cpp`: 6 risky callback comparisons.
- `Code/Scripts/Toolkit.cpp`: 6 risky callback comparisons.
- `Code/Scripts/Mission10.cpp`: 5 risky callback comparisons.
- `Code/Scripts/Mission03.cpp`: 4 risky callback comparisons.

Top attachment-count examples:

- `Code/Scripts/Mission09.cpp:4041` `M09_Innate_Disable`: 84 attachments, 2 damage comparisons.
- `Code/Scripts/Mission10.cpp:4208` `M10_Con_Yard_Repair`: 22 attachments, 1 damage comparison.
- `Code/Scripts/Mission05.cpp:7428` `M05_APC_Deploy`: 21 attachments, 1 damage comparison.
- `Code/Scripts/Mission03.cpp:5911` `M03_Zone_Enabled_Spawner`: 19 attachments, 1 enterer comparison.
- `Code/Scripts/Mission01.cpp:17423` `M01_Use_Ladder_Zone_JDG`: 11 attachments, 1 enterer comparison.

## Fixed-Origin `Get_A_Star` Uses

These attached scripts call `Commands->Get_A_Star(Vector3(0,0,0))`:

- `Code/Scripts/Mission01.cpp:2638` `M01_Ambient_Sound_Controller_JDG`: 2 hits.
- `Code/Scripts/Mission01.cpp:3396` `M01_HON_Dorm_Crapper_JDG`: 1 hit.
- `Code/Scripts/Mission03.cpp:1679` `M03_Ambient_Birdcall_Controller_JDG`: 1 hit.
- `Code/Scripts/Mission05.cpp:456` `M05_Activate_Objective_502`: 1 hit.
- `Code/Scripts/Mission05.cpp:775` `M05_Activate_Objective_510`: 1 hit.
- `Code/Scripts/Mission05.cpp:832` `M05_DEAD6_Engineer`: 1 hit.
- `Code/Scripts/Mission06.cpp:1374` `M06_Alarm_Controller`: 1 hit.
- `Code/Scripts/Toolkit_Triggers.cpp:100` `M00_Trigger_When_Killed_RMV`: 1 hit.

The `M05_*` conversation cases can use the actual `enterer` or `poker`. `M00_Trigger_When_Killed_RMV` uses explicit target IDs in all CSV attachments, so its fixed-origin fallback does not appear active for shipped campaign use.

## Missing Script Declarations

27 CSV attachment rows, covering 18 unique script names, did not resolve to an active `DECLARE_SCRIPT(...)` in the current source scan. Several are commented-out declarations in mission files. This looks like existing level/source drift rather than a new co-op issue, but these should be kept in mind when testing affected missions.

## Remaining Recommended Script Pass

1. Review the remaining fixed-origin `Get_A_Star(Vector3(0,0,0))` cases that do not have an obvious local actor.
2. Leave general AI targeting and movement uses of `STAR` alone for now; nearest-player behavior is usually desirable there.
3. Test M01, M03, M05, M09, M10, and M11 first.
