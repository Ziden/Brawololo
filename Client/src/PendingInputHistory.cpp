#include "Client/PendingInputHistory.hpp"

#include <algorithm>
#include <utility>

namespace game::client {

void PendingInputHistory::Record(game::ClientInputPacket packet)
{
    unackedInputs_.push_back(std::move(packet));
}

void PendingInputHistory::AcknowledgeThrough(game::CommandSequence sequence)
{
    const auto newEnd = std::remove_if(
        unackedInputs_.begin(),
        unackedInputs_.end(),
        [sequence](const game::ClientInputPacket& packet) {
            return packet.header.sequence <= sequence;
        });
    unackedInputs_.erase(newEnd, unackedInputs_.end());
}

const std::vector<game::ClientInputPacket>& PendingInputHistory::UnackedInputs() const noexcept
{
    return unackedInputs_;
}

std::size_t PendingInputHistory::Size() const noexcept
{
    return unackedInputs_.size();
}

void PendingInputHistory::Clear()
{
    unackedInputs_.clear();
}

} // namespace game::client
