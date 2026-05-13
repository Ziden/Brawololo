#include "RaylibDebugOverlay.hpp"

#include <raylib.h>

namespace game::client {
namespace {

const char* ConnectionStateName(ClientConnectionState state) {
    switch (state) {
        case ClientConnectionState::Disconnected:
            return "Disconnected";
        case ClientConnectionState::Connecting:
            return "Connecting";
        case ClientConnectionState::AwaitingSpawn:
            return "Awaiting spawn";
        case ClientConnectionState::Connected:
            return "Connected";
    }

    return "Unknown";
}

} // namespace

void DrawRaylibDebugOverlay(const ClientViewFrame& frame, int viewportHeight) {
    DrawText("Raylib Multiplayer Scaffold", 24, 24, 24, RAYWHITE);
    DrawText("Client view renders a view model, not raw networking/server state.",
             24,
             56,
             18,
             LIGHTGRAY);
    DrawText(TextFormat("Tick: %llu | Entities: %zu | Unacked: %zu | Snapshot: %u",
                        static_cast<unsigned long long>(frame.stats.fixedTick),
                        frame.stats.entityCount,
                        frame.stats.unackedInputCount,
                        frame.stats.session.lastSnapshotId),
             24,
             84,
             18,
             LIGHTGRAY);
    DrawText(TextFormat("Presentation: %u -> %u | alpha %u/1000",
                        frame.presentation.fromSnapshotId,
                        frame.presentation.toSnapshotId,
                        frame.presentation.alphaPermille),
             24,
             110,
             18,
             LIGHTGRAY);
    DrawText(TextFormat("Connection: %s | Snapshot ACKs: %zu | Reliable event ACKs: %zu",
                        ConnectionStateName(frame.stats.session.connectionState),
                        frame.stats.session.snapshotAcksSent,
                        frame.stats.session.reliableEventAcksSent),
             24,
             136,
             18,
             LIGHTGRAY);

    auto eventY = 180;
    for (const auto& line : frame.eventLines) {
        DrawText(line.c_str(), 24, eventY, 16, Color{255, 236, 179, 255});
        eventY += 22;
        if (eventY > viewportHeight - 32) {
            break;
        }
    }
}

} // namespace game::client
