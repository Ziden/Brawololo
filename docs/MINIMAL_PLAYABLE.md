# Minimal Playable Gate

The current minimal-playable gate is non-visual and server-authoritative. It is intentionally stricter than a renderer smoke test.

Run:

```powershell
cmake --build --preset vs2022-debug --target TwoClientSmoke
.\build\vs2022-debug\Client\Debug\TwoClientSmoke.exe
```

Or through CTest:

```powershell
ctest --preset vs2022-debug -R "Smoke.MinimalPlayable"
```

The gate requires:

- Two real `ClientApplication` instances.
- Two `RemoteClientSession`s.
- One `ServerNetworkHost`.
- Routed transport through `MultiClientLoopbackTransport`.
- Login/spawn, input, snapshots, and snapshot ACKs.
- Bow warmup, server-owned projectiles, hits, damage, deaths, and respawns.

This is not a replacement for manual Raylib playtesting. It is the fast regression check that the playable loop still crosses the same client/server boundary as future remote clients.
