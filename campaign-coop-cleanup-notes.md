# Campaign Co-op Cleanup Notes

## Raw Notes

- Co-op host menu still shows "Host LAN" at the top; it should use co-op/campaign-specific title text.
- Co-op menu should support direct in-game connection: fields for IP/address, port, and player name instead of requiring command-line `--coop-connect`.
- Co-op difficulty settings should display named difficulty labels instead of raw numeric values.
- Reuse the existing character/data-entry viewing/cycling systems as a co-op character model selector. Players should be able to open character selection and swap their model at any point during co-op.
- Co-op needs client support for campaign level transitions so host and connected clients load the next mission together.
- Clients should see objective markers and objective progress the same way the host does, including the small circular objective icons.
- Add a co-op teammate location marker so each player can track the other player's position.
- Add an in-game co-op menu option to manually respawn.
- Consider exposing the existing multiplayer score HUD in co-op if campaign/co-op score values are already calculated.
- Where co-op currently shows a blank character/player name field, display the player's actual network/player name.
- During cinematics/scripted sequences, when the host/player 1 controls are locked, clients should be locked too.
- Investigate whether enemy dodge behavior is disabled/missing in co-op; first verify whether this is tied to campaign difficulty settings.
- Add optional co-op gameplay movement features: sprint using the dsh/dash animations and cut roll animations, with gameplay options to enable or disable sprint and rolls.
- Fix and implement the existing leg twist code so character lower-body movement can better follow movement direction.

## Plan Candidates

- 

## Open Questions

- Are campaign/co-op score values already calculated through the multiplayer scoring path, or would co-op need new scoring hooks?
- Are enemy dodge behaviors difficulty-gated in normal campaign, or did co-op initialization disable an AI behavior path?
- What are the exact asset/animation names for the dsh/dash sprint animations and cut roll animations?
