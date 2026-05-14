# Raylib Multiplayer Scaffold

This is a C++ codebase for a server-authoritative multiplayer game using shared EnTT ECS game logic, a Raylib client, a headless server, and transport abstractions prepared for KCP over libdatachannel running a WASM client on the browser.

## Layout
- `GameLogic/`: shared ECS simulation, system-split movement/combat logic, weapon definitions, pure commands, domain events, network event DTOs, DTO snapshots, baseline cache, interest tracking, lag-compensation queries, replication planning, prediction/reconciliation helpers, fixed-point movement, map chunks, and time sync.
- `Networking/`: transport lifecycle interfaces, channel/message policy, point-to-point loopback, peer-addressed multi-client loopback, optional loss/reorder conditions, byte-level transport packet codec, RTC signaling/data-channel seams, diagnostics, and the KCP/libdatachannel adapter seam.
- `Client/`: Raylib app shell, fixed-step host loop, client application orchestration, session adapters, protocol pump, renderer-agnostic view models/effects, split Raylib scene/debug rendering, connection state, snapshot/reliable-event acknowledgements, presentation interpolation, pending-input history, local prediction, and reconciliation.
- `Server/`: headless server shell, network host, per-client replication state, authoritative runtime, stale command rejection, snapshot/reliable-event acknowledgement handling, reliable event broadcasting, baseline-aware snapshot generation, AOI interest diagnostics, lag-compensation history, and simulation-only test drivers.
- `tests/`: dependency-light test harness covering the core architecture.

The `Client` executable is Raylib-only. It drives `ClientApplication`, which talks to an `IClientSession` instead of knowing whether the server is in-process or remote. `ClientProtocolPump`, `ServerNetworkHost`, `InProcessClientSession`, and `RemoteClientSession` share the same serialized message path. The `SingleProcess` executable is only a compact bridge/smoke target for that boundary, with optional simulation-only opponents driven by normal server input packets.

`TwoClientSmoke` is a non-visual multiplayer workflow target. It runs two `ClientApplication` instances over two `RemoteClientSession`s, routes both through one `ServerNetworkHost` with `MultiClientLoopbackTransport`, and fails if clients do not connect, exchange snapshots, submit inputs, and produce server-authoritative combat events.

`RtcSmoke` is a non-visual RTC-shaped workflow target. It runs one `RemoteClientSession` and one `ServerNetworkHost` through `KcpRtcPumpedTransport` with in-memory signaling/data-channel backends, then fails if login, input, snapshots, and ACK flow do not cross that boundary.

The browser client uses a separate Emscripten entrypoint and `LocalPreviewClientSession`. It previews the Raylib client, input, fixed tick, local prediction, and view-model path without embedding a server or pretending KCP/WebRTC is already live.

## Build
The intended path is CMake:

```powershell
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug

```

On Windows, the checked-in `native-*` presets require `Ninja` plus a visible C++ compiler. If you have Visual Studio 2022 or Build Tools installed but not Ninja, use the Visual Studio preset instead:

```powershell
cmake --preset vs2022-debug
cmake --build --preset vs2022-debug --target SingleProcess
.\build\vs2022-debug\Client\Debug\SingleProcess.exe
```

Raylib is required whenever `SCAFFOLD_BUILD_CLIENT=ON`. By default CMake fetches Raylib when it is not already installed. To build only non-client targets without Raylib, configure with `-DSCAFFOLD_BUILD_CLIENT=OFF`.

```powershell
cmake --build --preset vs2022-debug --target Client
.\build\vs2022-debug\Client\Debug\Client.exe
```

For the routed two-client smoke workflow:

```powershell
cmake --build --preset vs2022-debug --target TwoClientSmoke
.\build\vs2022-debug\Client\Debug\TwoClientSmoke.exe
```

For the RTC-shaped remote transport smoke workflow:

```powershell
cmake --build --preset vs2022-debug --target RtcSmoke
.\build\vs2022-debug\Client\Debug\RtcSmoke.exe
```

For the browser preview workflow on Windows, use the repo-local runner instead of hand-assembling EMSDK commands each time:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\RunBrowserPreview.ps1
```

That script will:
- locate `emsdk_env.ps1` from `EMSDK` or common install paths,
- configure and build the `emscripten-client` preset,
- serve `build/emscripten-client/Client/Client.html` over HTTP with `emrun` when available,
- fall back to `python -m http.server` when `emrun` is unavailable.

If EMSDK is not installed yet, install and activate it first:

```powershell
git clone https://github.com/emscripten-core/emsdk.git "$env:USERPROFILE\emsdk"
Set-Location "$env:USERPROFILE\emsdk"
.\emsdk install latest
.\emsdk activate latest
setx EMSDK "$env:USERPROFILE\emsdk"
```

The browser target is currently a human preview path, not a full multiplayer validation path. It builds the Emscripten client preset in [CMakePresets.json](CMakePresets.json), launches the browser-specific entry point in [Client/src/browser_main.cpp](Client/src/browser_main.cpp), and currently uses `LocalPreviewClientSession` rather than a real networked browser transport.

For the minimal-playable native gate:

```powershell
ctest --preset vs2022-debug -R "Smoke.MinimalPlayable"
```

For the browser preview build, activate Emscripten so `EMSDK` is set, then run:

```powershell
cmake --preset emscripten-client
cmake --build --preset emscripten-client --target Client
```

See [Browser/WASM Build Notes](docs/BROWSER_WASM.md) for the current browser networking constraints.

## Tooling
Repository-level editor and static-analysis settings live in `.editorconfig`, `.clang-format`, and `.clang-tidy`. CMake also emits `compile_commands.json` for presets that support it so clang-based tools can resolve the same build flags as the compiler.

For sanitizers on Clang/GCC-compatible native builds:

```powershell
cmake --preset native-asan
cmake --build --preset native-asan --target ScaffoldTests
ctest --preset native-asan
```

For MSVC static analysis:

```powershell
cmake --preset vs2022-analysis
cmake --build --preset vs2022-analysis --target ScaffoldTests
```

For clang-tidy with a Clang toolchain available on `PATH`:

```powershell
cmake --preset native-clang-tidy
cmake --build --preset native-clang-tidy --target ScaffoldTests
```

GitHub Actions in `.github/workflows/ci.yml` runs the Windows debug build and dedicated analysis/sanitizer jobs so the local presets and CI stay aligned.

## Architecture Notes
The simulation is fixed tick and integer/fixed-point based for reconciliation and browser/native consistency. It does not attempt global cross-platform lockstep determinism. The server owns combat truth, arrows, hit detection, damage, death, and respawn; the client predicts only the local player and visual feedback, interpolates remote presentation state, then reconciles from authoritative snapshots. Time sync, input replay history, and lag-compensation lookup are explicit scaffold seams rather than hidden inside rendering or transport code.

Bow is implemented as the first `WeaponDefinition`, not as a hard-coded special case. Players carry `WeaponStateComponent`, so adding another weapon should primarily mean adding a weapon definition and its server-authoritative fire behavior.

The Raylib scene consumes renderer-agnostic `ClientViewFrame` data. It currently provides a camera-follow tile-map scaffold, player/projectile rendering, health bars, warmup rings, defeated-state visuals, cosmetic predicted fire cues, authoritative hit/death/respawn effects, and prediction-correction markers without reaching into networking or server internals.

Reliable event delivery is app-level and testable: clients ACK reliable network events, and the server resends pending events with bounded backoff until acknowledged.

Replication hardening now includes delta placeholder planning. The server compares ACKed snapshot baselines with the next planned snapshot and records added/changed/removed/unchanged entity counts, but the wire serializer intentionally still sends full-state snapshots until real delta encoding is added.

AOI transitions are also explicit: snapshot interest enter/leave transitions emit reliable interest events so spawn/despawn policy is visible and testable outside local client snapshot application.

`KcpRtcTransport` is still a scaffold seam, not a live KCP/WebRTC implementation. It now has explicit role/config fields, async signaling state, outgoing signaling DTOs, encoded data-channel frame queues, incoming frame decode hooks, and safe invalid-configuration/not-connected/protocol-rejected behavior.

`IRtcSignalingClient` is the signaling boundary. `InMemoryRtcSignalingClient` and `InMemoryRtcSignalingHub` give tests and native tools a real signaling service path for serialized `RtcSignalingMessage`s, while `PumpKcpRtcSignaling` bridges that signaling path into `KcpRtcTransport`.

`IRtcDataChannel` is the data-channel byte-frame boundary. `InMemoryRtcDataChannel` gives tests and native tools a paired frame channel, while `PumpKcpRtcDataChannel` bridges encoded frames between that channel and `KcpRtcTransport`.

`KcpRtcPumpedTransport` composes `KcpRtcTransport`, `IRtcSignalingClient`, and `IRtcDataChannel` as a normal `ITransport`. The in-memory pair factory powers `RtcSmoke`; `CreateRemoteTransport` can construct the pumped shape with explicit unsupported native backends that report `NotImplemented` until a real libdatachannel backend exists.

`CreateRemoteTransport` is the remote transport factory boundary. It preserves a single place for native/browser client code to request either the raw RTC scaffold seam or the pumped RTC composition.

`RemoteClientSession` supports async transports: it starts the transport, waits for `Connected`, and only then sends the login request. That keeps the client session compatible with WebRTC-style connection timing.
