#include "GameLogic/Commands.hpp"
#include "GameLogic/Map.hpp"
#include "GameLogic/NetworkProtocol.hpp"
#include "GameLogic/ReplicationPlanner.hpp"
#include "GameLogic/Serialization.hpp"
#include "GameLogic/Simulation.hpp"
#include "GameLogic/TimeSync.hpp"
#include "Networking/LoopbackTransport.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>

namespace {

using test_support::ContainsEvent;
using test_support::Expect;
using test_support::TestEntity;

void TestPureCommandsAndChannels()
{
    using namespace game;

    Expect(std::is_trivially_copyable_v<CommandHeader>, "CommandHeader is pure data");
    Expect(std::is_trivially_copyable_v<InputFrame>, "InputFrame is pure data");
    Expect(std::is_trivially_copyable_v<ClientInputPacket>, "ClientInputPacket is pure data");
    Expect(PolicyFor(NetworkChannel::MovementInput).reliability == Reliability::Unreliable, "movement/input is unreliable");
    Expect(PolicyFor(NetworkChannel::Snapshots).reliability == Reliability::UnreliableSequenced, "snapshots are unreliable sequenced");
    Expect(PolicyFor(NetworkChannel::CombatEvents).reliability == Reliability::Reliable, "combat events are reliable");
    Expect(PolicyFor(NetworkChannel::LoginSpawn).reliability == Reliability::Reliable, "login/spawn is reliable");
    Expect(PolicyFor(NetworkChannel::ChatUi).reliability == Reliability::Reliable, "chat/UI is reliable");

    NetworkEnvelope valid{};
    valid.channel = NetworkChannel::MovementInput;
    valid.messageClass = MessageClass::ClientInput;
    Expect(ValidateEnvelope(valid).Ok(), "valid envelope passes protocol validation");

    auto badVersion = valid;
    badVersion.protocolVersion = 999;
    Expect(ValidateEnvelope(badVersion).error == ProtocolError::UnsupportedVersion, "protocol rejects unsupported version");

    auto badChannel = valid;
    badChannel.channel = NetworkChannel::Snapshots;
    Expect(ValidateEnvelope(badChannel).error == ProtocolError::ChannelMismatch, "protocol rejects channel mismatch");

    auto tooLarge = valid;
    tooLarge.payload.resize(kMaxNetworkPayloadBytes + 1);
    Expect(ValidateEnvelope(tooLarge).error == ProtocolError::PayloadTooLarge, "protocol rejects oversized payload");
}

void TestSpawnMoveAndDomainEvents()
{
    using namespace game;

    GameSimulation simulation{};
    LoginCommand login{};
    login.header.clientId = 1;
    Expect(simulation.Submit(login), "login accepted");
    Expect(ContainsEvent<PlayerSpawned>(simulation.DrainEvents()), "spawn emits PlayerSpawned");

    const auto entityId = simulation.PlayerEntityForClient(1);
    Expect(entityId.has_value(), "spawn created player entity");
    const auto before = simulation.MovementForEntity(*entityId);

    ClientInputPacket input{};
    input.header.clientId = 1;
    input.header.sequence = 1;
    input.input.moveX = 1;
    Expect(simulation.Submit(input), "input accepted");
    simulation.TickFixed();

    const auto after = simulation.MovementForEntity(*entityId);
    Expect(after.has_value() && before.has_value() && after->x > before->x, "fixed-point movement advances player");
    Expect(ContainsEvent<PlayerMoved>(simulation.DrainEvents()), "movement emits PlayerMoved");
}

void TestBowWarmupAndServerOwnedProjectile()
{
    using namespace game;

    GameSimulation simulation{};
    LoginCommand login{};
    login.header.clientId = 1;
    Expect(simulation.Submit(login), "login accepted for bow test");
    (void)simulation.DrainEvents();

    ClientInputPacket input{};
    input.header.clientId = 1;
    input.header.sequence = 1;
    input.input.fire = true;
    Expect(simulation.Submit(input), "fire input accepted");
    simulation.TickFixed();
    Expect(ContainsEvent<WeaponWarmupStarted>(simulation.DrainEvents()), "fire starts warmup event");

    for (int i = 0; i < 20; ++i) {
        simulation.TickFixed();
    }

    const auto events = simulation.DrainEvents();
    Expect(ContainsEvent<BowFired>(events), "server emits BowFired after warmup");
    Expect(ContainsEvent<ProjectileSpawned>(events), "server spawns authoritative projectile");
}

void TestSnapshotsAndSerialization()
{
    using namespace game;

    GameSimulation simulation{};
    LoginCommand login{};
    login.header.clientId = 7;
    Expect(simulation.Submit(login), "login accepted for snapshot test");

    const auto snapshot = simulation.BuildSnapshot(7, 42, 41);
    Expect(snapshot.snapshotId == 42, "snapshotId is reserved");
    Expect(snapshot.baselineId == 41, "baselineId is reserved");
    Expect(!snapshot.entities.empty(), "snapshot includes entity state DTOs");
    Expect(snapshot.entities.front().replication.priority == SnapshotPriority::High, "owned player has high priority");

    const auto bytes = SerializeMovementState(snapshot.entities.front().movement);
    const auto decoded = DeserializeMovementState(std::span<const std::byte>{bytes.data(), bytes.size()});
    Expect(decoded.has_value(), "movement DTO round-trips");
    Expect(decoded->entityId == snapshot.entities.front().movement.entityId, "movement DTO preserves entity id");

    ClientInputPacket input{};
    input.header.clientId = 7;
    input.header.sequence = 99;
    input.header.clientTimestampMs = 1234;
    input.input.moveX = 1;
    input.input.aimY = 1000;
    input.input.fire = true;

    const auto inputBytes = SerializeClientInputPacket(input);
    const auto decodedInput = DeserializeClientInputPacket(std::span<const std::byte>{inputBytes.data(), inputBytes.size()});
    Expect(decodedInput.has_value(), "client input packet round-trips");
    Expect(decodedInput->header.sequence == 99, "client input sequence preserved");
    Expect(decodedInput->input.fire, "client input fire flag preserved");

    const auto snapshotBytes = SerializeSnapshot(snapshot);
    const auto decodedSnapshot = DeserializeSnapshot(std::span<const std::byte>{snapshotBytes.data(), snapshotBytes.size()});
    Expect(decodedSnapshot.has_value(), "snapshot DTO round-trips");
    Expect(decodedSnapshot->snapshotId == 42, "snapshot round-trip preserves snapshotId");
    Expect(decodedSnapshot->baselineId == 41, "snapshot round-trip preserves baselineId");
    Expect(decodedSnapshot->entities.size() == snapshot.entities.size(), "snapshot round-trip preserves entity count");

    LoginCommand loginRoundTrip{};
    loginRoundTrip.header.clientId = 3;
    loginRoundTrip.header.sequence = 0;
    loginRoundTrip.header.clientTimestampMs = 77;
    const auto loginBytes = SerializeLoginCommand(loginRoundTrip);
    const auto decodedLogin = DeserializeLoginCommand(std::span<const std::byte>{loginBytes.data(), loginBytes.size()});
    Expect(decodedLogin.has_value(), "login command round-trips");
    Expect(decodedLogin->header.clientId == 3, "login round-trip preserves client id");
}

void TestReplicationPlannerPrioritizesAndBudgetsSnapshots()
{
    game::SnapshotDTO snapshot{};
    snapshot.snapshotId = 10;
    snapshot.baselineId = 9;
    snapshot.entities.push_back(TestEntity(game::NetworkEntityId{5}, 0, game::SnapshotPriority::Medium));
    snapshot.entities.push_back(TestEntity(game::NetworkEntityId{3}, 0, game::SnapshotPriority::High));
    snapshot.entities.push_back(TestEntity(game::NetworkEntityId{2}, 0, game::SnapshotPriority::Low));
    snapshot.entities.push_back(TestEntity(game::NetworkEntityId{9}, 7, game::SnapshotPriority::Medium));

    game::ReplicationPlanner planner{game::ReplicationPlannerConfig{
        2,
        true,
        true,
    }};

    const auto planned = planner.Plan(snapshot, 7);
    Expect(planned.snapshot.snapshotId == 10, "replication planner preserves snapshot metadata");
    Expect(planned.snapshot.entities.size() == 2, "replication planner applies entity budget");
    Expect(planned.snapshot.entities[0].replication.ownerClientId == 7, "replication planner keeps owner first");
    Expect(planned.snapshot.entities[1].replication.priority == game::SnapshotPriority::High, "replication planner keeps high priority next");
    Expect(planned.stats.inputEntityCount == 4, "replication planner records input count");
    Expect(planned.stats.outputEntityCount == 2, "replication planner records output count");
    Expect(planned.stats.droppedEntityCount == 2, "replication planner records dropped count");
}

void TestMapAoiTimeSyncAndLoopback()
{
    using namespace game;

    MapConfig map{};
    const auto center = ChunkForPosition(map, PixelsToFixed(128 * 12), PixelsToFixed(128 * 12));
    const auto nearby = ChunkForPosition(map, PixelsToFixed(128 * 15), PixelsToFixed(128 * 15));
    const auto far = ChunkForPosition(map, PixelsToFixed(128 * 70), PixelsToFixed(128 * 70));
    Expect(IsChunkInAreaOfInterest(map, center, nearby), "near chunk is in AOI");
    Expect(!IsChunkInAreaOfInterest(map, center, far), "far chunk is outside AOI");

    NetworkClock clock{};
    clock.RecordSample(TimeSyncSample{100, 180, 220});
    Expect(clock.RttMs() == 120, "time sync records RTT");
    Expect(clock.EstimateServerTime(200) > 200, "time sync maps client time to server time");

    auto [clientTransport, serverTransport] = game::net::LoopbackTransport::CreatePair();
    ClientInputPacket input{};
    input.header.clientId = 22;
    input.header.sequence = 12;
    input.header.clientTimestampMs = 444;
    input.input.moveX = -1;

    NetworkEnvelope envelope{};
    envelope.channel = NetworkChannel::MovementInput;
    envelope.messageClass = MessageClass::ClientInput;
    envelope.sequence = input.header.sequence;
    envelope.payload = SerializeClientInputPacket(input);
    Expect(clientTransport.Send(envelope), "loopback send succeeds");
    const auto received = serverTransport.Poll();
    Expect(received.has_value() && received->sequence == 12, "loopback poll receives envelope");
    const auto decodedInput = received.has_value()
        ? DeserializeClientInputPacket(std::span<const std::byte>{received->payload.data(), received->payload.size()})
        : std::optional<ClientInputPacket>{};
    Expect(decodedInput.has_value() && decodedInput->header.clientId == 22, "loopback payload decodes input packet");
}

} // namespace

TEST(GameLogicTests, PureCommandsAndChannels)
{
    TestPureCommandsAndChannels();
}

TEST(GameLogicTests, SpawnMoveAndDomainEvents)
{
    TestSpawnMoveAndDomainEvents();
}

TEST(GameLogicTests, BowWarmupAndServerOwnedProjectile)
{
    TestBowWarmupAndServerOwnedProjectile();
}

TEST(GameLogicTests, SnapshotsAndSerialization)
{
    TestSnapshotsAndSerialization();
}

TEST(GameLogicTests, ReplicationPlannerPrioritizesAndBudgetsSnapshots)
{
    TestReplicationPlannerPrioritizesAndBudgetsSnapshots();
}

TEST(GameLogicTests, MapAoiTimeSyncAndLoopback)
{
    TestMapAoiTimeSyncAndLoopback();
}
