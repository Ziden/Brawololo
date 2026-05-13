Multiplayer Raylib Game Scaffold Plan
Summary
Create PROJECT_PLAN.md for a greenfield C++ multiplayer scaffold split into GameLogic, Client, Server, and Tests. The game uses shared EnTT ECS simulation, Raylib rendering/input, KCP + libdatachannel networking, and same-process loopback mode for fast single-player iteration.

The architecture targets server-authoritative consistency, not cross-platform lockstep determinism. Simulation correctness takes priority over visual smoothness. Simulation runs on fixed ticks with integer/fixed-point movement data, while rendering runs separately with interpolation.

Public Interfaces And Data Flow
GameLogic exposes a fixed-tick simulation API that accepts pure-data commands, advances simulation time, and emits specific domain events for UI/render reactions.
Avoid generic ComponentChangedEvent; use events such as PlayerSpawned, PlayerMoved, WeaponWarmupStarted, BowFired, ProjectileSpawned, HitConfirmed, EntityEnteredInterest, EntityLeftInterest, SnapshotApplied, and LocalPredictionCorrected.
Commands are serializable data only, with at minimum clientId, sequence, clientTimestamp, and command-specific payload.
Core command/input structures include InputFrame and ClientInputPacket, plus login/spawn, movement intent, aim direction, and fire weapon commands.
Snapshot and replication DTOs are ECS-independent: prefer MovementStateDTO, CombatStateDTO, and ReplicationStateDTO over serializing ECS component types directly.
Component data is still represented in the wire model conceptually, but through stable DTOs, network entity IDs, and local EnTT entity mapping rather than raw ECS storage.
Core Implementation
GameLogic: ECS components/systems, integer/fixed-point movement, command handlers, event queue, prediction helpers, reconciliation helpers, serialization DTOs, map/chunk AOI, bow warmup, arrows, and server-authoritative projectile/combat rules.
Client: sample input into sequence-numbered ClientInputPackets, predict local movement/action feedback, render from ECS state, react to domain events, interpolate remote entities, and reconcile only local predicted state from server snapshots.
Server: authoritative tick loop, command validation, input ack tracking, stale command rejection, lag-compensation history, server-owned arrows and hit detection, prioritized component-state snapshots, and chunk-filtered AOI replication for up to 256 players.
Networking: define message classes and channels: movement/input unreliable, snapshots unreliable sequenced, combat events reliable, login/spawn reliable, chat/UI reliable.
Time: explicit NetworkClock/TimeSync module for RTT estimate, client/server clock offset, interpolation delay, and mapping client command timestamps to server time.
Replication: reserve snapshotId and baselineId in snapshot messages from day one, even if initial delta compression sends full state.
Netcode Rules
Separate simulation time from render time: simulation advances in fixed ticks; rendering uses alpha/interpolation against buffered states.
Prediction roles are explicit via components/tags such as PredictedTag, InterpolatedTag, and AuthoritativeTag.
Prediction applies only to the local controlled player and local visual feedback; clients never own arrows, hit detection, or combat truth.
Reconciliation uses authoritative snapshots plus acknowledged command sequence numbers, rewinding/replaying only the local predicted state needed for correction.
No full-world rollback, no lockstep rollback, and no rollback of all players.
Remote entities are rendered through interpolation buffers.
Server lag compensation uses bounded historical state to validate movement, bow 300ms warmup, firing, projectile timing, and hit checks.
Snapshot prioritization is explicit: high priority for nearby players, combat, and projectiles; medium for movement; low for cosmetics and far entities.
Test Plan
Unit tests cover pure command handling, domain event emission, integer/fixed-point movement, rotation, bow warmup, projectile spawning, prediction tags, ownership metadata, serialization DTOs, chunk lookup, and AOI filtering.
Netcode tests cover channels/message classes, sequence numbers, stale rejection, snapshot/baseline IDs, prediction, local-only reconciliation replay, remote interpolation, clock offset/RTT estimates, packet loss, and out-of-order delivery.
Server tests cover login spawn, 256 simulated players, server-authoritative arrows/hits, lag-compensated bow validation, prioritized snapshots, and AOI-scoped replication.
Integration tests run client and server in one process through loopback transport and verify movement, rotation, firing, domain event delivery, snapshots, time sync, and reconciliation.
Assumptions
KCP + libdatachannel replaces ENet for the initial scaffold.
Browser support is designed around WebRTC-style data channels rather than raw UDP.
Single-player means an authoritative server and client running in the same process through loopback transport.
Simulation correctness and server authority are preferred over hiding latency with client-owned gameplay truth.
The first scaffold targets a clean vertical slice and testable architecture, not matchmaking, persistence, accounts, anti-cheat, or production deployment.