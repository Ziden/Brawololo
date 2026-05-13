#include "Networking/LoopbackTransport.hpp"
#include "Server/ServerApplication.hpp"

#include <iostream>
#include <memory>

int main() {
    auto transports = game::net::LoopbackTransport::CreatePair();
    auto serverTransport =
        std::make_unique<game::net::LoopbackTransport>(std::move(transports.second));

    game::server::ServerApplication server{std::move(serverTransport)};
    if (!server.Start()) {
        std::cerr << "Failed to start server application\n";
        return 1;
    }

    server.RunForTicks(1);
    const auto stats = server.Stats();
    server.RequestStop();

    std::cout << "Server application smoke ran. Ticks: " << stats.ticksRun
              << ", network clients: " << stats.network.connectedClients << '\n';
    return 0;
}
