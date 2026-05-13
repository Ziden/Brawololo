#include "Client/ClientApplication.hpp"
#include "Client/InProcessClientSession.hpp"
#include "RaylibClientHost.hpp"

#include <memory>

int main() {
    auto session = std::make_unique<game::client::InProcessClientSession>();
    game::client::ClientApplication app{std::move(session),
                                        game::client::ClientApplicationConfig{1}};
    game::client::RaylibClientHost host{};
    return host.Run(app);
}
