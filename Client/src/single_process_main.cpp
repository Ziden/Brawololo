#include "Client/ClientApplication.hpp"
#include "Client/InProcessClientSession.hpp"

#include <iostream>
#include <memory>

int main()
{
    auto session = std::make_unique<game::client::InProcessClientSession>();
    game::client::ClientApplication app{std::move(session), game::client::ClientApplicationConfig{1}};

    if (!app.Connect(0)) {
        std::cerr << "Failed to initialize single-process bridge\n";
        return 1;
    }

    for (int tick = 0; tick < 30; ++tick) {
        game::InputFrame input{};
        input.moveX = 1;
        input.fire = tick == 1;

        const auto nowMs = static_cast<game::TimestampMs>(tick * 16);
        (void)app.SubmitInput(input, nowMs);
        app.TickFixed(nowMs);
        (void)app.DrainEvents();
    }

    const auto stats = app.Stats();
    std::cout << "Single-process bridge ran. Entities: "
              << stats.entityCount
              << ", snapshots applied: "
              << stats.session.snapshotsApplied
              << '\n';
    return 0;
}

