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
- `ServerNetworkHost` now computes snapshot delta placeholder stats from ACKed baselines while preserving the current full-state serializer.
- `ServerNetworkHost` sends reliable AOI enter/leave events for spawn/despawn policy at the interest boundary.
- `Server` also owns `SimulationOnlyClientDriver`, a deterministic test/play driver that submits normal pure `ClientInputPacket`s for simulation-only opponents.
- `Networking` owns transport lifecycle abstractions, protocol envelope validation, loopback transport with deterministic loss/delay conditions, and the placeholder KCP/libdatachannel seam.
- `Networking` now owns `TransportPacketCodec`, the byte-level frame codec that a real data channel adapter will use to carry validated `NetworkEnvelope`s.
- `Networking` now owns `RtcSignalingMessage`, a typed signaling DTO for join/offer/answer/ICE/ready/leave/error exchange before a data channel is usable.
- `Networking` now owns `IRtcSignalingClient`, `InMemoryRtcSignalingClient`, and `InMemoryRtcSignalingHub`, giving native tests/tools a real signaling service path that routes serialized signaling DTOs by session and peer target.
- `Networking` now owns `PumpKcpRtcSignaling`, the adapter that moves queued `KcpRtcTransport` signals through an `IRtcSignalingClient` and feeds received signals back into the transport.
- `Networking` now owns `IRtcDataChannel`, `InMemoryRtcDataChannel`, and `PumpKcpRtcDataChannel`, giving the RTC transport a tested byte-frame IO seam for future libdatachannel callbacks.
- `Networking` now owns explicit unsupported native RTC backends that report `NotImplemented` through the normal `ITransport` error path until real native/browser implementations exist.
- `KcpRtcTransport` exposes `NotifyDataChannelReady`, the explicit callback a real WebRTC data-channel backend should call when its channel opens.
- `Networking` now owns `KcpRtcPumpedTransport` and `CreateInMemoryKcpRtcTransportPair`, a full `ITransport` wrapper that pumps signaling and data-channel frames around `KcpRtcTransport`.
- `Networking` now includes `MultiClientLoopbackTransport`, a peer-addressed loopback transport that routes multiple remote client endpoints through one server transport for architecture tests and future local workflows.
- `KcpRtcTransport` has explicit role/config fields, async signaling state, outgoing signaling queues, encoded outgoing data-channel frames, and incoming frame decode hooks for the future libdatachannel/KCP adapter.
- `Networking` exposes `CreateRemoteTransport`, a factory seam that constructs raw or pumped RTC transports without leaking KCP/WebRTC details into client code.
- `RemoteClientSession` supports async transport connection and defers login until the transport reports `Connected`.
- Same-process mode runs client and server through loopback transport and the same serialized protocol path as remote networking.
- Browser builds use a separate Raylib/Emscripten entrypoint with `LocalPreviewClientSession`, so the client can compile without `ServerCore` and preview local prediction while real WebRTC transport is pending.
- `RtcSmoke` verifies one real `RemoteClientSession` and one `ServerNetworkHost` can exchange login, input, snapshots, and ACKs through the pumped RTC transport path.
- Current native verification has been green with `cmake --build --preset vs2022-debug`, `ctest --preset vs2022-debug`, `Server.exe`, `SingleProcess.exe`, and `TwoClientSmoke.exe`.

## Current Gameplay
- Players can connect/spawn, move with fixed-point acceleration, aim, and fire the bow.
- Bow has a warmup and spawns server-owned arrows.
- Projectiles move server-side, confirm hits, apply damage, kill defeated players, and trigger timed respawns.
- Client predicts the local player and reconciles from authoritative snapshots.
- Remote entities render through presentation interpolation.
- Raylib scene rendering now has a camera-follow tile-map world scaffold, player/projectile drawing, health bars, warmup rings, defeated-state visuals, and event text.
- Raylib scene rendering also shows cosmetic predicted fire cues, authoritative hit/damage bursts, death pulses, respawn pulses, and prediction-correction markers from view effects.
- Same-process mode can spawn a simulation-only opponent that aims/fires through pure server-side input packets for quick combat testing.

## Missing For Final Playable
- Wire live libdatachannel/KCP peer callbacks behind the existing `KcpRtcTransport` signaling/frame/data-channel boundary.
- Replace or complement the in-memory signaling path with a real native/browser signaling backend.
- Replace or complement the in-memory data-channel path with real libdatachannel-backed frame IO.

## Milestones To Final Playable Version
1. Done: combat outcome loop with bow hit damage, death state, respawn timer, health snapshots, and damage/death/respawn domain events.
2. Done: client scene pass with camera follow, tile-map/world rendering scaffold, clearer player/projectile/health visualization, and defeated-state rendering.
3. Done: same-process combat test loop with a server-owned simulation-only opponent submitting ordinary input packets.
4. Done: combat UX polish with cosmetic predicted fire feedback, authoritative hit/death/respawn effects, and prediction-correction markers.
5. Done: reliable-event resend/backoff for unacked app-level reliable events.
6. Done: peer-addressed multi-client loopback support so two real `RemoteClientSession`s can share one `ServerNetworkHost`.
7. Done: real multiplayer smoke workflow through `TwoClientSmoke`, using two `ClientApplication` instances and two `RemoteClientSession`s against one `ServerNetworkHost`.
8. Done: replication hardening with snapshot delta placeholders and reliable AOI enter/leave spawn/despawn events.
9. Done: remote transport scaffold pass with safer `KcpRtcTransport` config/state/error behavior and peer-addressed fake remote workflow through multi-client loopback.
10. Done: Browser/WASM build-path hardening with Emscripten entrypoint, shell file, local preview session, main-loop support, and browser networking docs.
11. Done: native remote transport construction/config seam through `CreateRemoteTransport` and safer async `KcpRtcTransport` lifecycle behavior.
12. Done: minimal playable native gate through `Smoke.MinimalPlayable` / `TwoClientSmoke`, covering login, movement, snapshots, bow combat, deaths, and respawns.
13. Done: remote transport packet/signaling runway with `TransportPacketCodec`, `RtcSignalingMessage`, `KcpRtcTransport` signaling/data-channel queues, and async `RemoteClientSession` login.
14. Partially done: native signaling service path through `IRtcSignalingClient`, `InMemoryRtcSignalingClient`, and `PumpKcpRtcSignaling`; live libdatachannel peer callbacks remain.
15. Partially done: data-channel byte IO path through `IRtcDataChannel`, `InMemoryRtcDataChannel`, and `PumpKcpRtcDataChannel`; live libdatachannel data-channel callbacks remain.
16. Done: pumped RTC transport wrapper through `KcpRtcPumpedTransport`, `NotifyDataChannelReady`, and `CreateInMemoryKcpRtcTransportPair`.
17. Done: RTC transport smoke workflow through `RtcSmoke`, covering `RemoteClientSession` plus `ServerNetworkHost` over the pumped RTC path.
18. Done: backend selection/error reporting through `RtcBackendKind`, unsupported native RTC backends, and pumped RTC factory mode.
19. Native remote transport implementation: wire actual libdatachannel callbacks and real signaling IO into the existing `KcpRtcTransport` boundary.
20. Browser multiplayer implementation: connect the Emscripten client to WebRTC data channels once `KcpRtcTransport` is live.

## Current Priority
The next implementation slice should attach real WebRTC/data-channel IO to the remote networking boundary:
- Add libdatachannel peer/data-channel callbacks that feed `ReceiveSignalingMessage`, `ReceiveFrame`, `PollOutgoingSignal`, and `PollOutgoingFrame`.
- Replace `RtcBackendKind::UnsupportedNative` with production signaling and data-channel backends that implement `IRtcSignalingClient` and `IRtcDataChannel`.
- Keep `CreateRemoteTransport` as the construction boundary for native and browser clients.
- Keep `TwoClientSmoke`, `Smoke.MinimalPlayable`, and `SingleProcess` as native regression checks while remote transport is implemented.
