#include "Client/ClientApplication.hpp"
#include "Client/LocalPreviewClientSession.hpp"
#include "RaylibClientHost.hpp"

#include <memory>

int main()
{
    auto session = std::make_unique<game::client::LocalPreviewClientSession>();
    auto* app = new game::client::ClientApplication{
        std::move(session),
        game::client::ClientApplicationConfig{1}};
    auto* host = new game::client::RaylibClientHost{game::client::RaylibClientHostConfig{
        1280,
        720,
        "Raylib Multiplayer Scaffold - Browser Preview"}};
    return host->Run(*app);
}
