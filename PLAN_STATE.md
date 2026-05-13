# Multiplayer Raylib Game Plan State

## Current State
The project is in a late scaffold/early playable-slice phase. It is not yet a final polished playable game, but the architecture now has the major seams needed to build one without turning the single-process mode or Raylib client into snowflakes.

## Implemented Architecture
- `GameLogic` owns shared ECS simulation, pure commands, domain events, fixed tick orchestration, fixed-point movement, map/chunk AOI, snapshots, serialization DTOs, weapon definitions, and server-authoritative projectile/combat rules.
- `GameLogic` simulation implementation is split by responsibility: `Simulation.cpp` for the facade/snapshots/entities, `SimulationMovement.cpp` for input and movement, and `SimulationCombat.cpp` for weapons/projectiles/hit checks.
- Weapons are data-driven through `WeaponDefinition` and `WeaponStateComponent`; bow is the first/default weapon rather than a hard-coded player component.
- `Client` owns Raylib host/input/rendering, fixed-step orchestration, prediction/reconciliation, presentation interpolation, pending input history, connection state, snapshot ACKs, and reliable event ACKs.
- `Client` also owns renderer-agnostic visual effect view data through `ClientVisualEffectLog`; Raylib draws these as cosmetic cues without changing simulation truth.
- `Server` owns authoritative runtime, server network host, lag-compensation history/query seam, per-client replication state, snapshot cadence, reliable event tracking, AOI interest tracking, and baseline-aware snapshot delivery.
- `ServerNetworkHost` resends pending reliable events on bounded exponential backoff until clients ACK them.
- `Server` also owns `SimulationOnlyClientDriver`, a deterministic test/play driver that submits normal pure `ClientInputPacket`s for simulation-only opponents.
- `Networking` owns transport lifecycle abstractions, protocol envelope validation, loopback transport with deterministic loss/delay conditions, and the placeholder KCP/libdatachannel seam.
- `Networking` now includes `MultiClientLoopbackTransport`, a peer-addressed loopback transport that routes multiple remote client endpoints through one server transport for architecture tests and future local workflows.
- Same-process mode runs client and server through loopback transport and the same serialized protocol path as remote networking.
- Current verification has been green with `cmake --build --preset vs2022-debug`, `ctest --preset vs2022-debug`, `Server.exe`, `SingleProcess.exe`, and `TwoClientSmoke.exe`.

## Current Gameplay
- Players can connect/spawn, move with fixed-point acceleration, aim, and fire the bow.
- Bow has a warmup and spawns server-owned arrows.
- Projectiles move server-side, confirm hits, apply damage, kill defeated players, and trigger timed respawns.
- Client predicts the local player and reconciles from authoritative snapshots.
- Remote entities render through presentation interpolation.
- Raylib scene rendering now has a camera-follow tile-map world scaffold, player/projectile drawing, health bars, warmup rings, defeated-state visuals, and event text.
- Raylib scene rendering also shows cosmetic predicted fire cues, authoritative hit/damage bursts, death pulses, respawn pulses, and prediction-correction markers from view effects.
- Same-process mode can spawn a simulation-only opponent that aims/fires through pure server-side input packets for quick combat testing.

## Missing For Minimal Playable
- Add a real remote transport adapter before browser/WASM networking work.

## Milestones To Final Playable Version
1. Done: combat outcome loop with bow hit damage, death state, respawn timer, health snapshots, and damage/death/respawn domain events.
2. Done: client scene pass with camera follow, tile-map/world rendering scaffold, clearer player/projectile/health visualization, and defeated-state rendering.
3. Done: same-process combat test loop with a server-owned simulation-only opponent submitting ordinary input packets.
4. Done: combat UX polish with cosmetic predicted fire feedback, authoritative hit/death/respawn effects, and prediction-correction markers.
5. Done: reliable-event resend/backoff for unacked app-level reliable events.
6. Done: peer-addressed multi-client loopback support so two real `RemoteClientSession`s can share one `ServerNetworkHost`.
7. Done: real multiplayer smoke workflow through `TwoClientSmoke`, using two `ClientApplication` instances and two `RemoteClientSession`s against one `ServerNetworkHost`.
8. Replication hardening: snapshot delta placeholder implementation and AOI enter/leave reliable spawn/despawn policy.
9. Remote transport pass: flesh out KCP/libdatachannel adapter or a fake remote socket adapter before browser work.
10. Browser/WASM pass: Raylib web build preset, asset packaging, browser-safe networking path, and smoke documentation.
11. Minimal playable release: one server, clients join instantly, move/aim/fire bow, arrows damage players, deaths respawn, and single-process mode remains a clean bridge.

## Current Priority
The next implementation slice should continue replication hardening:
- Add snapshot delta placeholders around baseline comparison without changing the current full-state serializer yet.
- Add explicit AOI enter/leave spawn/despawn policy using reliable events.
- Keep `TwoClientSmoke` as the quick end-to-end workflow check while hardening replication.
