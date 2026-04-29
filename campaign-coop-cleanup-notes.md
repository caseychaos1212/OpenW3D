# Campaign Co-op Cleanup Notes

## Raw Notes

- Co-op host menu still shows "Host LAN" at the top; it should use co-op/campaign-specific title text.
- Co-op menu should support direct in-game connection: fields for IP/address, port, and player name instead of requiring command-line `--coop-connect`.
- Co-op difficulty settings should display named difficulty labels instead of raw numeric values.
- Reuse the existing character/data-entry viewing/cycling systems as a co-op character model selector. Players should be able to open character selection and swap their model at any point during co-op.
- Co-op needs client support for campaign level transitions so host and connected clients load the next mission together.
- Clients should see objective markers and objective progress the same way the host does, including the small circular objective icons.
- Clients could see some objects that were hidden from the host, likely objective-related; investigate replication of script/object hide-show state.
- Add a co-op teammate location marker so each player can track the other player's position.
- Add an in-game co-op menu option to manually respawn.
- Consider exposing the existing multiplayer score HUD in co-op if campaign/co-op score values are already calculated; player deaths should subtract points.
- Where co-op currently shows a blank character/player name field, display the player's actual network/player name.
- During cinematics/scripted sequences, when the host/player 1 controls are locked, clients should be locked too.
- Investigate whether enemy dodge behavior is disabled/missing in co-op; first verify whether this is tied to campaign difficulty settings.
- Add optional co-op gameplay movement features: sprint using the dsh/dash animations and cut roll animations, with gameplay options to enable or disable sprint and rolls.
- Fix and implement the existing leg twist code so character lower-body movement can better follow movement direction.

## Implementation Plan

### Phase 1: Co-op Menu And Connection UX

Goal: make co-op discoverable and usable without command-line arguments.

- Fix co-op host dialog title/subtitle text so it no longer says "Host LAN" and consistently uses co-op campaign wording.
- Replace raw numeric difficulty display with named campaign difficulty labels.
- Add an in-game direct connect flow with fields for address/IP, port, and player name.
- Reuse the existing co-op connection path behind the dialog so command-line `--coop-connect` and in-game direct connect share the same code.
- Validate and persist direct connect fields using the same config/user options style as existing multiplayer dialogs.

Test focus:

- Host co-op from the menu and verify all visible text is co-op/campaign-specific.
- Join co-op from the menu using `ip:port`.
- Verify command-line direct connect still works.
- Verify invalid address/port/name inputs produce clear menu feedback.

### Phase 2: Campaign State Synchronization

Goal: make clients see and experience mission state the same way the host does.

- Finish server-authoritative co-op level transitions so the host picks the next campaign mission and all clients unload/load together.
- Audit objective marker/progress systems and replicate the state needed for clients to see objective icons and progress updates.
- Investigate objective-related object visibility where clients see hidden objects; identify whether hide/show is driven by scripts, object state bits, or client-only mission logic.
- Replicate or force-refresh script/object hide-show state for clients after join and during mission updates.
- Synchronize scripted control locks so clients are locked during cinematics whenever the host/player 1 is locked.

Test focus:

- Complete a co-op mission and verify host/client both load the same next mission.
- Trigger objective updates and verify host/client HUD markers match.
- Verify hidden objective/script objects remain hidden on clients.
- Trigger tutorial/cinematic control locks and verify both players lose/regain control together.

### Phase 3: Co-op HUD, Identity, And Respawn UX

Goal: make co-op feel like a two-player campaign instead of a multiplayer mode awkwardly attached to campaign.

- Display each player's actual network/player name where the co-op HUD/name field is currently blank.
- Add a teammate location marker so each player can track the other player.
- Add an in-game co-op menu action for manual respawn.
- Decide how manual respawn interacts with the safe ally-spawn system and combat-pressure rules.
- Expose the multiplayer score HUD in co-op if score values are already available.
- Add player death score penalties if co-op scoring is enabled.

Test focus:

- Verify both players see correct names locally and over the network.
- Verify teammate marker tracks the other player and does not conflict with objective markers.
- Verify manual respawn works while alive, dead, in combat, and out of combat.
- Verify score changes for kills/objectives if available, and score subtracts on player death.

### Phase 4: Character Model Selection

Goal: let players choose and change campaign co-op character models without breaking gameplay state.

Status: first implementation in progress on the co-op branch.

- Find the existing character/data-entry viewer cycling code and identify the reusable model preview pieces.
- Build a co-op character selection dialog using that preview/cycle behavior.
- Define a co-op-safe model swap path that changes visual model without resetting health, inventory, weapons, objectives, player type, or network ownership.
- Add server validation so clients can only select allowed co-op character models.
- Replicate selected model changes to all peers and late joiners.
- Current approach: reuse the EVA Characters data-entry tab, show all character entries in co-op, add a co-op-only Select button, send a client-to-server selection event, and apply the selected soldier preset as a replicated model-only override.

Test focus:

- Open character selection during co-op and swap models on host/client.
- Verify health, weapons, inventory, and mission script state survive model swaps.
- Verify the selected model replicates to the other player and persists across respawn.
- Verify invalid/missing model choices fail gracefully.

### Phase 5: Movement And AI Polish

Goal: add optional feel improvements after core synchronization is solid.

- Add optional sprint support using the dsh/dash animation set once exact asset names are verified.
- Add optional roll support using the cut roll animations once exact asset names are verified.
- Add gameplay options to enable/disable sprint and rolls.
- Investigate whether enemy dodge behavior is difficulty-gated in normal campaign or disabled by co-op initialization.
- If co-op disables dodge behavior, restore the same AI behavior path used by normal campaign.

Test focus:

- Verify sprint/roll options save, load, and replicate where needed.
- Verify sprint/roll animations exist for selected player models and fail safely when missing.
- Compare enemy dodge behavior in normal campaign and co-op at each difficulty.

## Suggested First Implementation Pass

1. Menu title/difficulty labels/direct connect fields.
2. Level transition synchronization.
3. Objective markers plus hidden-object visibility replication.
4. Cinematic/control lock synchronization.
5. Player names, teammate marker, and manual respawn.

Character selection, co-op scoring, sprint/rolls, and AI dodge cleanup should come after the synchronization pass unless one of them blocks testing.

## Open Questions

- Are campaign/co-op score values already calculated through the multiplayer scoring path, or would co-op need new scoring hooks?
- Are enemy dodge behaviors difficulty-gated in normal campaign, or did co-op initialization disable an AI behavior path?
- What are the exact asset/animation names for the dsh/dash sprint animations and cut roll animations?
