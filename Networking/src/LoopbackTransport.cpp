#include "Networking/LoopbackTransport.hpp"

namespace game::net {

std::pair<LoopbackTransport, LoopbackTransport> LoopbackTransport::CreatePair()
{
    return CreatePair({});
}

std::pair<LoopbackTransport, LoopbackTransport> LoopbackTransport::CreatePair(
    LoopbackNetworkConditions conditions)
{
    auto state = std::make_shared<SharedState>();
    return {
        LoopbackTransport{state, Endpoint::A, conditions},
        LoopbackTransport{state, Endpoint::B, conditions},
    };
}

LoopbackTransport::LoopbackTransport(
    std::shared_ptr<SharedState> state,
    Endpoint endpoint,
    LoopbackNetworkConditions conditions)
    : state_(std::move(state))
    , endpoint_(endpoint)
    , conditions_(conditions)
    , stateValue_(TransportState::Connected)
{
}

void LoopbackTransport::SetNetworkConditions(LoopbackNetworkConditions conditions) noexcept
{
    conditions_ = conditions;
}

void LoopbackTransport::FlushDelayed()
{
    if (!state_) {
        return;
    }

    while (!DelayedOutgoingQueue().empty()) {
        ReleaseOneDelayed();
    }
}

bool LoopbackTransport::Connect()
{
    if (!state_) {
        stateValue_ = TransportState::Failed;
        lastError_ = TransportError::NotConnected;
        return false;
    }

    stateValue_ = TransportState::Connected;
    lastError_ = TransportError::None;
    return true;
}

void LoopbackTransport::Update()
{
    ReleaseOneDelayed();
}

void LoopbackTransport::Close()
{
    stateValue_ = TransportState::Disconnected;
}

bool LoopbackTransport::Send(const game::NetworkEnvelope& envelope)
{
    if (!state_ || stateValue_ != TransportState::Connected) {
        lastError_ = TransportError::NotConnected;
        ++stats_.sendFailures;
        return false;
    }

    if (!game::ValidateEnvelope(envelope).Ok()) {
        lastError_ = TransportError::ProtocolRejected;
        ++stats_.sendFailures;
        return false;
    }

    ++outgoingEnvelopeOrdinal_;
    if (ShouldDropOutgoing()) {
        ++stats_.envelopesDroppedByCondition;
        lastError_ = TransportError::None;
        return true;
    }

    if (ShouldDelayOutgoing()) {
        DelayedOutgoingQueue().push(envelope);
        ++stats_.envelopesDelayedByCondition;
        lastError_ = TransportError::None;
        return true;
    }

    PushOutgoing(envelope);
    ReleaseOneDelayed();
    lastError_ = TransportError::None;
    return true;
}

std::optional<game::NetworkEnvelope> LoopbackTransport::Poll()
{
    if (!state_ || stateValue_ != TransportState::Connected || IncomingQueue().empty()) {
        return std::nullopt;
    }

    auto envelope = IncomingQueue().front();
    IncomingQueue().pop();
    if (!game::ValidateEnvelope(envelope).Ok()) {
        lastError_ = TransportError::ProtocolRejected;
        ++stats_.receiveFailures;
        return std::nullopt;
    }

    ++stats_.envelopesReceived;
    stats_.bytesReceived += envelope.payload.size();
    lastError_ = TransportError::None;
    return envelope;
}

TransportState LoopbackTransport::State() const noexcept
{
    return stateValue_;
}

TransportStats LoopbackTransport::Stats() const noexcept
{
    return stats_;
}

TransportError LoopbackTransport::LastError() const noexcept
{
    return lastError_;
}

std::queue<game::NetworkEnvelope>& LoopbackTransport::IncomingQueue()
{
    return endpoint_ == Endpoint::A ? state_->bToA : state_->aToB;
}

std::queue<game::NetworkEnvelope>& LoopbackTransport::OutgoingQueue()
{
    return endpoint_ == Endpoint::A ? state_->aToB : state_->bToA;
}

std::queue<game::NetworkEnvelope>& LoopbackTransport::DelayedOutgoingQueue()
{
    return endpoint_ == Endpoint::A ? state_->delayedAToB : state_->delayedBToA;
}

bool LoopbackTransport::ShouldDropOutgoing() const noexcept
{
    return conditions_.dropEveryNthOutgoingEnvelope != 0 &&
        outgoingEnvelopeOrdinal_ % conditions_.dropEveryNthOutgoingEnvelope == 0;
}

bool LoopbackTransport::ShouldDelayOutgoing() const noexcept
{
    return conditions_.delayEveryNthOutgoingEnvelope != 0 &&
        outgoingEnvelopeOrdinal_ % conditions_.delayEveryNthOutgoingEnvelope == 0;
}

void LoopbackTransport::PushOutgoing(game::NetworkEnvelope envelope)
{
    stats_.bytesSent += envelope.payload.size();
    ++stats_.envelopesSent;
    OutgoingQueue().push(std::move(envelope));
}

void LoopbackTransport::ReleaseOneDelayed()
{
    if (!state_ || DelayedOutgoingQueue().empty()) {
        return;
    }

    auto delayed = DelayedOutgoingQueue().front();
    DelayedOutgoingQueue().pop();
    PushOutgoing(std::move(delayed));
}

} // namespace game::net
