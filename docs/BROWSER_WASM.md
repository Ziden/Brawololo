# Browser/WASM Build Notes

The browser client is a Raylib/Emscripten build of the client presentation and local prediction loop. It intentionally does not embed the server.

## Build

Install and activate Emscripten so `EMSDK` points at the SDK root, then run:

```powershell
cmake --preset emscripten-client
cmake --build --preset emscripten-client --target Client
```

The output is `build/emscripten-client/Client/Client.html`.

## Current Browser Mode

- The browser entrypoint uses `LocalPreviewClientSession`.
- It runs the same `ClientApplication`, Raylib view, fixed tick, input sampling, local prediction, and view-model path as native.
- It does not fake authoritative combat, snapshots, or server ACKs.
- Multiplayer networking for browsers is reserved for WebRTC data channels behind `KcpRtcTransport`.

## Networking Path

Raw UDP is not browser-safe. Browser multiplayer should use:

- WebRTC data channels for browser transport.
- KCP-style reliability/ordering policy inside or above the data channel where useful.
- `RtcSignalingMessage` for join/offer/answer/ICE/data-channel-ready exchange.
- `IRtcSignalingClient` as the boundary a browser signaling backend should implement.
- `IRtcDataChannel` as the boundary browser WebRTC data-channel callbacks should implement.
- `TransportPacketCodec` for byte-level `NetworkEnvelope` frames once a data channel is open.
- `NetworkEnvelope::peerId` only as routing metadata, never gameplay identity.
- `ClientInputPacket`, snapshots, reliable events, ACKs, and time sync exactly as native clients use them.

`KcpRtcTransport` is currently a safe scaffold seam. It validates config/protocol/state, starts async signaling, queues outgoing signaling DTOs, and encodes/decodes data-channel frames. `PumpKcpRtcSignaling` can move those signals through any `IRtcSignalingClient`; `PumpKcpRtcDataChannel` can move encoded frames through any `IRtcDataChannel`. The browser still needs real signaling and WebRTC data-channel backends.

`KcpRtcPumpedTransport` proves the shape of that composition for native tests by wrapping a `KcpRtcTransport`, signaling client, and data channel as one `ITransport`. A browser implementation should use the same composition idea, with browser-backed `IRtcSignalingClient` and `IRtcDataChannel` implementations.

Until those browser/native backends exist, the factory-supported production backend kind is explicit unsupported behavior: connection attempts fail with `TransportError::NotImplemented` rather than silently falling back to fake networking.
