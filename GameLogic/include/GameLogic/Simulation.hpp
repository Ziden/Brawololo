#pragma once

#include "GameLogic/Commands.hpp"
#include "GameLogic/Components.hpp"
#include "GameLogic/Events.hpp"
#include "GameLogic/Map.hpp"
#include "GameLogic/Replication.hpp"

#include <entt/entt.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace game {

enum class SimulationMode {
    ServerAuthoritative,
    ClientPrediction,
};

struct SimulationConfig {
    SimulationMode mode{SimulationMode::ServerAuthoritative};
    MapConfig map{};
    TimestampMs tickDurationMs{16};
    std::uint16_t maxPlayers{256};
    Fixed movementAccelerationPerSecond{PixelsToFixed(2600)};
    Fixed maxMoveSpeedPerSecond{PixelsToFixed(360)};
    std::int32_t maxPlayerHealth{100};
    TimestampMs respawnDelayMs{1500};
    WeaponType defaultWeapon{WeaponType::Bow};
    WeaponDefinitionTable weapons{DefaultWeaponDefinitions()};
};

class GameSimulation {
public:
    explicit GameSimulation(SimulationConfig config = {});

    [[nodiscard]] bool Submit(const LoginCommand& command);
    [[nodiscard]] bool Submit(const ClientInputPacket& packet);
    void TickFixed();
    void ReplayLocalInputsForPrediction(const std::vector<ClientInputPacket>& inputs);

    [[nodiscard]] SnapshotDTO BuildSnapshot(ClientId observerClientId, SnapshotId snapshotId, SnapshotId baselineId) const;
    void ApplySnapshot(const SnapshotDTO& snapshot, ClientId localClientId);

    [[nodiscard]] EventList DrainEvents();
    [[nodiscard]] Tick CurrentTick() const noexcept;
    [[nodiscard]] TimestampMs ServerTimeMs() const noexcept;
    [[nodiscard]] std::optional<NetworkEntityId> PlayerEntityForClient(ClientId clientId) const;
    [[nodiscard]] std::optional<MovementStateDTO> MovementForEntity(NetworkEntityId entityId) const;
    [[nodiscard]] std::size_t EntityCount() const noexcept;

    [[nodiscard]] entt::registry& Registry() noexcept;
    [[nodiscard]] const entt::registry& Registry() const noexcept;

private:
    [[nodiscard]] NetworkEntityId AllocateNetworkId();
    [[nodiscard]] TransformComponent SpawnTransformForClient(ClientId clientId) const;
    [[nodiscard]] NetworkEntityId CreatePlayer(ClientId clientId, Fixed x, Fixed y);
    [[nodiscard]] NetworkEntityId CreateProjectile(
        NetworkEntityId ownerId,
        const TransformComponent& ownerTransform,
        const AimComponent& aim,
        const WeaponDefinition& weapon);
    [[nodiscard]] entt::entity EntityFor(NetworkEntityId entityId) const;

    void QueueEvent(DomainEvent event);
    void ApplyInputToEntity(entt::entity entity, const ClientInputPacket& packet);
    void IntegrateMovement(entt::entity entity);
    void ProcessInputs();
    void ProcessWeaponWarmups();
    void ProcessProjectiles();
    void ProcessRespawns();
    void ClampToMap(TransformComponent& transform) const;
    [[nodiscard]] SnapshotPriority PriorityForEntity(ClientId observerClientId, entt::entity entity) const;

    SimulationConfig config_{};
    entt::registry registry_{};
    Tick tick_{};
    TimestampMs timeMs_{};
    NetworkEntityId nextNetworkId_{1};
    EventList events_{};
    std::vector<ClientInputPacket> pendingInputs_{};
    std::unordered_map<ClientId, NetworkEntityId> playerByClient_{};
    std::unordered_map<NetworkEntityId, entt::entity> entityByNetworkId_{};
    std::unordered_map<ClientId, CommandSequence> lastAcceptedSequence_{};
};

} // namespace game
