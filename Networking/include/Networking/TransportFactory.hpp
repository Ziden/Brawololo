#pragma once

#include "Networking/ITransport.hpp"
#include "Networking/KcpRtcTransport.hpp"

#include <memory>

namespace game::net {

enum class RemoteTransportKind {
    KcpRtc,
    KcpRtcPumped,
};

enum class RtcBackendKind {
    UnsupportedNative,
};

struct RemoteTransportConfig {
    RemoteTransportKind kind{RemoteTransportKind::KcpRtc};
    KcpRtcTransportConfig kcpRtc{};
    RtcBackendKind rtcBackend{RtcBackendKind::UnsupportedNative};
};

[[nodiscard]] std::unique_ptr<ITransport> CreateRemoteTransport(const RemoteTransportConfig& config);

} // namespace game::net
