# Multiplayer Raylib Game Scaffold Plan

## Summary
This repository is scaffolded as a greenfield C++ multiplayer game split into `GameLogic`, `Client`, `Server`, `Networking`, and `Tests`. It uses shared EnTT ECS simulation code, Raylib rendering/input on the client, a KCP + libdatachannel-oriented transport seam, and a loopback transport for same-process client/server iteration.

The architecture targets server-authoritative consistency, not cross-platform lockstep determinism. Simulation correctness takes priority over visual smoothness. Simulation runs on fixed ticks with integer/fixed-point movement data, while rendering runs separately with interpolation.

## Public Interfaces And Data Flow
- `GameLogic` exposes a fixed-tick simulation API that accepts pure-data commands, advances simulation time, and emits specific domain events for UI/render reactions.
- Commands are serializable data only. Command headers contain at least `clientId`, `sequence`, and `clientTimestampMs`.
- Core input structures include `InputFrame` and `ClientInputPacket`.
- Events are domain-specific, such as `PlayerSpawned`, `PlayerMoved`, `WeaponWarmupStarted`, `BowFired`, `ProjectileSpawned`, `HitConfirmed`, `PlayerDamaged`, `PlayerDied`, `PlayerRespawned`, `EntityEnteredInterest`, `EntityLeftInterest`, `SnapshotApplied`, and `LocalPredictionCorrected`.
- Snapshot and replication DTOs are ECS-independent: use `MovementStateDTO`, `CombatStateDTO`, and `ReplicationStateDTO` instead of serializing ECS component types directly.
- Component data is represented in the wire model conceptually through DTOs, stable network entity IDs, and local EnTT mappings rather than raw ECS memory.

## Core Implementation
- `GameLogic`: ECS components/systems, integer/fixed-point movement, command handlers, event queue, data-driven weapon definitions, network event DTOs, local prediction helpers, reconciliation helpers, serialization DTOs, map/chunk AOI, bow warmup, arrows, damage/death/respawn, and server-authoritative projectile/combat rules.
- `Client`: sequence-numbered `ClientInputPacket`s, snapshot acknowledgements, reliable-event acknowledgements, connection-state tracking, local prediction, render-time interpolation, event-driven view hooks, renderer-agnostic cosmetic combat effects, split Raylib scene/debug rendering, routed two-client smoke workflow, and local-only reconciliation replay.
- `Server`: authoritative tick loop, command validation, input ack tracking, snapshot ack tracking, reliable event delivery tracking with bounded resend/backoff, per-client replication state, stale command rejection, lag-compensation history, server-owned arrows/hits, simulation-only test drivers, prioritized snapshots, per-client snapshot cadence, and chunk-filtered AOI replication for up to `256` players.
- `Networking`: message classes and channels for movement/input, snapshots, combat events, login/spawn, interest events, time sync, and chat/UI. The first concrete transports are point-to-point loopback and peer-addressed multi-client loopback; the KCP/libdatachannel adapter is isolated behind `ITransport`.
- `Time`: `NetworkClock`, `TimeSyncRequest`, and `TimeSyncResponse` support RTT estimates, client/server offset, interpolation delay, and client timestamp to server time mapping.
- `Replication`: snapshots reserve `snapshotId`, `baselineId`, and delivery kind from day one, even when the first implementation sends full state.

## Netcode Rules
- Simulation time is separate from render time.
- Prediction roles are explicit with `PredictedTag`, `InterpolatedTag`, and `AuthoritativeTag`.
- Bow is the first weapon definition; weapon state is generic so future weapons do not require player-specific component rewrites.
- Prediction applies only to the local controlled player and local visual feedback.
- Clients never own arrows, hit detection, or combat truth.
- Reconciliation uses authoritative snapshots plus acknowledged command sequence numbers, rewinding/replaying only local predicted state.
- Snapshot baselines are driven by client acknowledgements, not merely by the last sent snapshot.
- Pending client input history is explicit so replay/reconciliation policy does not live in ad hoc vectors.
- No full-world rollback, no lockstep rollback, and no rollback of all players.
- Server lag compensation uses bounded historical state and a dedicated query service for movement, bow warmup, firing, projectile timing, and hit checks.
- Snapshot prioritization is explicit: high priority for nearby players, combat, and projectiles; medium for movement; low for cosmetics and far entities.
- Reliable network events are DTO-based and preserve the domain event vocabulary for UI reactions without exposing transport or ECS internals.
- Reliable event acknowledgements are explicit so app-level delivery state can be tested independently of the eventual transport implementation.
- Unacknowledged reliable events are resent on bounded exponential backoff until acknowledged.

## Test Plan
- Unit tests cover pure command handling, domain event emission, fixed-point movement, bow warmup, projectile spawning, prediction tags, ownership metadata, serialization DTOs, chunk lookup, and AOI filtering.
- Netcode tests cover channels/message classes, sequence numbers, stale rejection, snapshot/baseline IDs, prediction, local-only reconciliation, interpolation buffers, clock offset/RTT estimates, packet loss, and out-of-order delivery.
- Server tests cover login spawn, 256 simulated players, server-authoritative arrows/hits, lag-compensated bow validation, prioritized snapshots, and AOI-scoped replication.
- Integration tests run client and server in one process through loopback transport.

## Assumptions
- KCP + libdatachannel replaces ENet for the initial scaffold.
- Browser support is designed around WebRTC-style data channels rather than raw UDP.
- Single-player means an authoritative server and client running in the same process through loopback transport.
- Simulation correctness and server authority are preferred over hiding latency with client-owned gameplay truth.
- The first scaffold targets a clean vertical slice and testable architecture, not matchmaking, persistence, accounts, anti-cheat, or production deployment.
