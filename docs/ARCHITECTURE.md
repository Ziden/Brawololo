# Architecture Notes

## Authority Model
The server owns gameplay truth. Clients may predict local movement and local visual feedback, but arrows, hit detection, combat outcomes, and authoritative positions come from server snapshots.

This project does not target cross-platform lockstep determinism. It targets server-authoritative consistency with fixed simulation ticks, integer/fixed-point movement, explicit command sequence numbers, and local-only reconciliation.

## Time Model
Simulation time and render time are separate. The simulation advances in fixed ticks. The client renders from current predicted local state plus presentation samples from `PresentationInterpolator`.

`NetworkClock` maps local render time to server time. The protocol now has explicit `TimeSyncRequest` and `TimeSyncResponse` DTOs on a `TimeSync` channel; sessions can exchange those packets without the simulation, Raylib view, or transport implementation knowing the byte-level format.

## Game Logic Contract
`GameLogic` accepts pure-data commands and emits domain events. UI and rendering code should react to specific events such as `PlayerSpawned`, `WeaponWarmupStarted`, `ProjectileSpawned`, `PlayerDamaged`, `PlayerDied`, `PlayerRespawned`, and `LocalPredictionCorrected`; it should not depend on a generic component-changed event stream.

`NetworkEventDTO` is the network-facing event model for reliable event delivery. It is derived from selected domain events and converted back into domain events on the client, so UI code still reacts to the same domain vocabulary without learning transport details.

The simulation facade remains `GameSimulation`, but implementation is split by responsibility:
- `Simulation.cpp` owns the public API, connection/input admission, entity identity maps, snapshot build/apply, and fixed-tick orchestration.
- `SimulationMovement.cpp` owns input intent, acceleration, fixed-point movement, friction, and map clamping.
- `SimulationCombat.cpp` owns weapon warmup completion, server-owned projectile spawning, projectile movement, hit checks, damage, death, respawn, and combat replication priority.

Weapons are data-driven through `WeaponDefinition` and `WeaponDefinitionTable`. `WeaponStateComponent` stores the currently equipped weapon and generic warmup state; bow is the default weapon definition rather than a hard-coded one-off component. `BowComponent` remains only as a compatibility alias for existing code/tests while new code should prefer `WeaponStateComponent`.

## Replication Contract
Wire data is ECS-independent. Snapshot messages use DTOs such as `MovementStateDTO`, `CombatStateDTO`, and `ReplicationStateDTO`. EnTT component layout is private to the simulation and can change without changing the protocol shape.

Snapshots reserve `snapshotId`, `baselineId`, and `SnapshotDeliveryKind` immediately so delta compression can be added later without reshaping all call sites. `SnapshotBaselineCache` tracks per-client snapshot baselines and decides whether a snapshot is currently full-state or delta-eligible; the first scaffold still serializes full state either way.

`SnapshotAckDTO` is the client-to-server baseline signal. The server bases delta eligibility on the latest acknowledged snapshot, not merely the latest snapshot it attempted to send. This is intentionally conservative under packet loss and delayed delivery.

`SnapshotDeltaPlan` is the current delta-compression placeholder. It compares a planned snapshot to an acknowledged baseline and classifies entity states as added, changed, removed, or unchanged. `ServerNetworkHost` records those counts for observability, but the serializer still sends full entity state until real delta payloads are introduced.

The first protocol serializer supports movement DTOs, `ClientInputPacket`, and `SnapshotDTO`. It is intentionally simple binary encoding for scaffold tests; the protocol boundary is explicit so versioning, endian policy, compression, and schema evolution can be added without touching ECS storage.

`ReplicationPlanner` is the explicit snapshot delivery policy seam. It consumes ECS-independent `SnapshotDTO` data, orders entities by owner/priority, applies optional entity budgets, and reports planning stats. `ServerNetworkHost` runs snapshots through this planner before serialization so future bandwidth limits, delta compression, and per-client scheduling do not leak into `GameSimulation`.

`InterestTracker` records per-client interest transitions from planned snapshots. The server can now distinguish entered/stayed/exited interest sets for diagnostics and future reliable spawn/despawn messaging without coupling that policy to EnTT storage.

AOI enter/leave is now an explicit reliable policy. When a planned snapshot changes a client's interest set, `ServerNetworkHost` sends observer-targeted `EntityEnteredInterest` and `EntityLeftInterest` network events. The client may still apply snapshot state immediately, but spawn/despawn intent is no longer only an incidental side effect of snapshot application.

## Networking Contract
Message classes are mapped to explicit channels:
- Movement/input: unreliable.
- Snapshots: unreliable sequenced.
- Snapshot acknowledgements: unreliable sequenced.
- Combat events: reliable.
- Login/spawn: reliable.
- Interest enter/leave events: reliable login/spawn channel.
- Reliable event acknowledgements: reliable login/spawn channel.
- Chat/UI: reliable.
- Time sync: unreliable.

All envelopes carry a protocol version and are validated before transport delivery or protocol pumping. The current schema reserves payload limits, channel/message compatibility checks, and version rejection as first-class behavior before a byte-level wire format is finalized.

`LoopbackTransport` is the concrete same-process point-to-point implementation for tests and fast iteration. It can optionally apply deterministic packet loss and delayed delivery through `LoopbackNetworkConditions`, giving tests a local way to exercise stale, missing, and out-of-order envelopes without depending on a real network stack. `MultiClientLoopbackTransport` adds peer-addressed routing so multiple `RemoteClientSession`s can share one `ServerNetworkHost` without leaking client routing into gameplay code. `KcpRtcTransport` is the integration seam for KCP over libdatachannel/WebRTC data channels.

`KcpRtcTransport` is intentionally still non-operational until libdatachannel/KCP integration is wired, but it now behaves like a real adapter boundary: valid configuration is distinguished from invalid configuration, protocol envelopes are validated before state checks, disconnected sends report `NotConnected`, and valid `Connect` attempts report `NotImplemented`.

`ITransport` exposes lifecycle and diagnostics: `Connect`, `Update`, `Close`, state, last error, and envelope/byte stats. Concrete transports should reject invalid envelopes and report `TransportError` instead of letting protocol problems leak into gameplay code.

Protocol pumping is split away from session/runtime code:
- `ClientProtocolPump` serializes login/input commands and applies snapshot envelopes to `ClientRuntime`.
- `ClientProtocolPump` also sends snapshot/reliable-event acknowledgements and ingests reliable network events and time-sync responses into session/runtime-facing structures.
- `ServerNetworkHost` owns server-side envelope handling, login/input/snapshot-ack/reliable-event-ack/time-sync processing, network-client snapshot delivery, reliable event broadcasting, and server network stats.
- `RemoteClientSession` is the scaffold for future real transports and already uses the same client protocol pump as in-process mode.

Server outbound ordering keeps unreliable sequenced snapshots ahead of reliable events generated in the same tick. This preserves snapshot-first reconciliation while still allowing UI/combat feedback to arrive through reliable event messages.

## Client And Single-Process Mode
The client layer is split into clear seams:
- `ClientRuntime` owns local prediction, reconciliation, interpolation buffers, and the client-side `GameLogic` simulation.
- `PendingInputHistory` owns unacknowledged input packets for local-only replay after authoritative snapshots.
- `PresentationInterpolator` buffers authoritative snapshots and produces renderer-facing `PresentationFrame` data with fixed-point interpolation for remote entities.
- `ClientApplication` orchestrates fixed ticks, input submission, runtime ticking, and session pumping.
- `IClientSession` hides whether snapshots/commands travel to an in-process server, a remote server, or a future browser transport. Session stats expose `ClientConnectionState` so UI/debug code can distinguish transport connection, awaiting spawn acknowledgement, and fully connected play state.
- `FixedStepClock` owns simulation-step scheduling so render frame rate does not drive simulation time.
- `ClientEventLog` keeps transient view feedback out of simulation and renderer internals.
- `ClientVisualEffectLog` converts domain events into short-lived renderer-agnostic cosmetic effects. Predicted fire cues are explicitly visual-only; authoritative hit, death, respawn, and correction effects come from simulation/network events.
- `ClientViewFrame` is renderer-agnostic view data built from runtime state, domain events, and presentation interpolation output.
- `RaylibClientHost` owns the Raylib platform loop and composes frame timing, input, app ticks, view-model building, and rendering.
- `RaylibGameView` samples Raylib input and owns the window frame; `RaylibSceneRenderer` draws the camera-follow tile-map scaffold, entities, health bars, warmup rings, defeated-state visuals, and cosmetic effects; `RaylibDebugOverlay` draws HUD/event text. None of these should talk to server/runtime internals directly.

Same-process mode is implemented as `InProcessClientSession`, an adapter that wires `ServerNetworkHost` and `LoopbackTransport` through the same serialized protocol path used by real networking. Simulation-only dummy clients can exist inside the server without becoming network clients that receive snapshots. `SimulationOnlyClientDriver` keeps this clean by submitting ordinary sequence-numbered `ClientInputPacket`s to `ServerRuntime` for those opponents. The bridge must not become a second client implementation.

`TwoClientSmoke` is the non-visual real-client workflow check. It composes two `ClientApplication` instances, two `RemoteClientSession`s, `MultiClientLoopbackTransport`, and one `ServerNetworkHost`; it does not bypass protocol pumping or use simulation-only clients.

## Server Shell
`ServerApplication` is the headless server shell. It owns transport lifecycle, constructs `ServerNetworkHost`, advances fixed server ticks, exposes server stats, and supports graceful shutdown through `RequestStop`. The `Server` executable should stay a composition entrypoint around this shell rather than talking directly to `ServerRuntime`.

`ServerRuntime` owns the historical snapshot buffer and exposes `QueryLagCompensation` through `LagCompensationService`. Input acceptance does not yet depend on that result; the seam is reserved for validating bow warmup, projectile timing, and hit checks against bounded historical state once combat rules become richer.

`ServerNetworkHostConfig::snapshotSendIntervalMs` controls per-client snapshot cadence. The default remains one snapshot per fixed tick, but the policy is now explicit and can be tuned independently of simulation tick rate.

`ServerClientReplicationState` owns per-client replication data: acknowledged snapshot baseline, next snapshot send time, AOI interest tracker, and pending reliable event deliveries. `NetworkEventAckDTO` removes pending reliable events once the client confirms ingestion. Pending reliable events resend with bounded exponential backoff until ACKed or until their configured send cap is reached.

`SimulationOnlyClientDriver` is a server-side test/play scaffold for local iteration. It reads authoritative simulation state only to produce normal pure-data commands for simulation-only clients, so its behavior exercises the same weapon warmup, projectile, damage, death, and respawn rules as any real client input.

`NetworkEnvelope::peerId` is transport routing metadata, not gameplay identity. Client commands still carry their own command headers, and server snapshots/events still target players by explicit DTO data; the peer id only lets multi-client transports route server responses to the right endpoint.
