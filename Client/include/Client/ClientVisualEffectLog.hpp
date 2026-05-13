#pragma once

#include "Client/ClientRuntime.hpp"
#include "Client/ClientViewModel.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace game::client {

struct ClientVisualEffectLogEntry {
    ViewEffect effect{};
    float secondsRemaining{};
    float lifetimeSeconds{};
};

class ClientVisualEffectLog {
public:
    explicit ClientVisualEffectLog(std::size_t maxEntries = 24);

    void PushFromEvents(const game::EventList& events, const ClientRuntime& runtime);
    void Update(float deltaSeconds);
    [[nodiscard]] std::vector<ViewEffect> Effects() const;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    void Push(ViewEffect effect, float lifetimeSeconds);
    [[nodiscard]] std::optional<ViewEffect> EffectAtEntity(
        ViewEffectKind kind,
        game::NetworkEntityId entityId,
        const ClientRuntime& runtime,
        std::int32_t amount = 0) const;

    std::size_t maxEntries_{};
    std::vector<ClientVisualEffectLogEntry> entries_{};
};

} // namespace game::client
