# Raylib Multiplayer Scaffold

This is a C++ scaffold for a server-authoritative multiplayer game using shared EnTT ECS game logic, a Raylib client, a headless server, and transport abstractions prepared for KCP over libdatachannel.

## Layout
- `GameLogic/`: shared ECS simulation, system-split movement/combat logic, weapon definitions, pure commands, domain events, network event DTOs, DTO snapshots, baseline cache, interest tracking, lag-compensation queries, replication planning, prediction/reconciliation helpers, fixed-point movement, map chunks, and time sync.
- `Networking/`: transport lifecycle interfaces, channel/message policy, loopback transport with optional loss/reorder conditions, diagnostics, and the KCP/libdatachannel adapter seam.
- `Client/`: Raylib app shell, fixed-step host loop, client application orchestration, session adapters, protocol pump, renderer-agnostic view models, split Raylib scene/debug rendering, connection state, snapshot/reliable-event acknowledgements, presentation interpolation, pending-input history, local prediction, and reconciliation.
- `Server/`: headless server shell, network host, per-client replication state, authoritative runtime, stale command rejection, snapshot/reliable-event acknowledgement handling, reliable event broadcasting, baseline-aware snapshot generation, AOI interest diagnostics, and lag-compensation history.
- `tests/`: dependency-light test harness covering the core architecture.

The `Client` executable is Raylib-only. It drives `ClientApplication`, which talks to an `IClientSession` instead of knowing whether the server is in-process or remote. `ClientProtocolPump`, `ServerNetworkHost`, `InProcessClientSession`, and `RemoteClientSession` share the same serialized message path. The `SingleProcess` executable is only a compact bridge/smoke target for that boundary.

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
The simulation is fixed tick and integer/fixed-point based for reconciliation and browser/native consistency. It does not attempt global cross-platform lockstep determinism. The server owns combat truth, arrows, and hit detection; the client predicts only the local player and visual feedback, interpolates remote presentation state, then reconciles from authoritative snapshots. Time sync, input replay history, and lag-compensation lookup are explicit scaffold seams rather than hidden inside rendering or transport code.

Bow is implemented as the first `WeaponDefinition`, not as a hard-coded special case. Players carry `WeaponStateComponent`, so adding another weapon should primarily mean adding a weapon definition and its server-authoritative fire behavior.
