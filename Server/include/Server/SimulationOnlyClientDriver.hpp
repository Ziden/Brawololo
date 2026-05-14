#pragma once

#include "GameLogic/Commands.hpp"
#include "GameLogic/Types.hpp"

namespace game::server {

struct SimulationOnlyClientDriverConfig {
    game::ClientId clientId{};
    game::ClientId targetClientId{};
    game::TimestampMs firstFireAtMs{1000};
    game::TimestampMs fireIntervalMs{1600};
};

class SimulationOnlyClientDriver {
public:
    explicit SimulationOnlyClientDriver(SimulationOnlyClientDriverConfig config);

    [[nodiscard]] game::ClientId ClientId() const noexcept;
    [[nodiscard]] bool Tick(class ServerRuntime& runtime, game::TimestampMs nowMs);

private:
    [[nodiscard]] game::InputFrame BuildInput(const class ServerRuntime& runtime,
                                              bool shouldFire) const;

    SimulationOnlyClientDriverConfig config_{};
    game::CommandSequence nextSequence_{1};
    game::TimestampMs nextFireAtMs_{};
};

} // namespace game::server
