#include "Networking/TransportFactory.hpp"

namespace game::net {

std::unique_ptr<ITransport> CreateRemoteTransport(const RemoteTransportConfig& config)
{
    switch (config.kind) {
    case RemoteTransportKind::KcpRtc:
        return std::make_unique<KcpRtcTransport>(config.kcpRtc);
    }

    return nullptr;
}

} // namespace game::net
