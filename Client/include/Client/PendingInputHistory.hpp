#pragma once

#include "GameLogic/Commands.hpp"

#include <cstddef>
#include <vector>

namespace game::client {

class PendingInputHistory {
public:
    void Record(game::ClientInputPacket packet);
    void AcknowledgeThrough(game::CommandSequence sequence);
    [[nodiscard]] const std::vector<game::ClientInputPacket>& UnackedInputs() const noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;
    void Clear();

private:
    std::vector<game::ClientInputPacket> unackedInputs_{};
};

} // namespace game::client
