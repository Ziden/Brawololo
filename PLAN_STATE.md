# Multiplayer Raylib Game Plan State

## Current State
The project is in a strong scaffold/hardening phase. It is not yet a final playable game, but the architecture now has the major seams needed to build one without turning the single-process mode or Raylib client into snowflakes.

## Implemented Architecture
- `GameLogic` owns shared ECS simulation, pure commands, domain events, fixed tick orchestration, fixed-point movement, map/chunk AOI, snapshots, serialization DTOs, weapon definitions, and server-authoritative projectile/combat rules.
- `GameLogic` simulation implementation is split by responsibility: `Simulation.cpp` for the facade/snapshots/entities, `SimulationMovement.cpp` for input and movement, and `SimulationCombat.cpp` for weapons/projectiles/hit checks.
- Weapons are data-driven through `WeaponDefinition` and `WeaponStateComponent`; bow is the first/default weapon rather than a hard-coded player component.
- `Client` owns Raylib host/input/rendering, fixed-step orchestration, prediction/reconciliation, presentation interpolation, pending input history, connection state, snapshot ACKs, and reliable event ACKs.
- `Server` owns authoritative runtime, server network host, lag-compensation history/query seam, per-client replication state, snapshot cadence, reliable event tracking, AOI interest tracking, and baseline-aware snapshot delivery.
- `Networking` owns transport lifecycle abstractions, protocol envelope validation, loopback transport with deterministic loss/delay conditions, and the placeholder KCP/libdatachannel seam.
- Same-process mode runs client and server through loopback transport and the same serialized protocol path as remote networking.
- Current verification has been green with `cmake --build --preset vs2022-debug`, `ctest --preset vs2022-debug`, `Server.exe`, and `SingleProcess.exe`.

## Current Gameplay
- Players can connect/spawn, move with fixed-point acceleration, aim, and fire the bow.
- Bow has a warmup and spawns server-owned arrows.
- Projectiles move server-side and currently confirm hits.
- Client predicts the local player and reconciles from authoritative snapshots.
- Remote entities render through presentation interpolation.

## Missing For Minimal Playable
- Apply damage on projectile hits, death, respawn, and health/death UI feedback.
- Render a readable tile-map playfield with camera follow instead of only debug dots on a flat background.
- Add a basic local UX loop: spawn, move, fire, see hits/deaths/respawns, and continue playing.
- Add simple server-side simulated players or a second-client workflow for testing combat without manual multi-client setup.

## Milestones To Final Playable Version
1. Combat outcome loop: bow hit damage, death state, respawn timer, health snapshots, and damage/death/respawn domain events.
2. Client scene pass: camera follow, tile-map/world rendering scaffold, clearer player/projectile/health visualization.
3. Combat UX pass: hit/death/respawn event text, simple health bars, warmup/fire feedback, local predicted fire FX only as visuals.
4. Multi-player test loop: either launch two clients against one server transport or add deterministic server-side dummy-player input for quick combat testing.
5. Replication hardening: queued reliable-event resend policy, snapshot delta placeholder implementation, and AOI enter/leave reliable spawn/despawn policy.
6. Remote transport pass: flesh out KCP/libdatachannel adapter or a fake remote socket adapter before browser work.
7. Browser/WASM pass: Raylib web build preset, asset packaging, browser-safe networking path, and smoke documentation.
8. Minimal playable release: one server, clients join instantly, move/aim/fire bow, arrows damage players, deaths respawn, and single-process mode remains a clean bridge.

## Current Priority
The next implementation slice should make the game loop visibly playable while preserving clean architecture:
- Add server-authoritative damage/death/respawn.
- Add health/death/respawn replication and domain events.
- Add camera-follow tile-map rendering in the Raylib scene layer.
