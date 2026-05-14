#include "Networking/TransportFactory.hpp"

#include "Networking/KcpRtcPumpedTransport.hpp"
#include "Networking/RtcDataChannel.hpp"
#include "Networking/RtcSignalingClient.hpp"

namespace game::net {
namespace {

std::unique_ptr<ITransport> CreatePumpedRtcTransport(const RemoteTransportConfig& config)
{
    switch (config.rtcBackend) {
    case RtcBackendKind::UnsupportedNative:
        return std::make_unique<KcpRtcPumpedTransport>(
            config.kcpRtc,
            std::make_unique<UnsupportedRtcSignalingClient>(),
            std::make_unique<UnsupportedRtcDataChannel>());
    }

    return nullptr;
}

} // namespace

std::unique_ptr<ITransport> CreateRemoteTransport(const RemoteTransportConfig& config)
{
    switch (config.kind) {
    case RemoteTransportKind::KcpRtc:
        return std::make_unique<KcpRtcTransport>(config.kcpRtc);
    case RemoteTransportKind::KcpRtcPumped:
        return CreatePumpedRtcTransport(config);
    }

    return nullptr;
}

} // namespace game::net
