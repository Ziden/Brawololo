#pragma once

#include "GameLogic/Events.hpp"
#include "GameLogic/Replication.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <variant>

namespace test_support {

inline void Expect(bool condition, std::string_view message)
{
    EXPECT_TRUE(condition) << message;
}

template <typename T>
bool ContainsEvent(const game::EventList& events)
{
    for (const auto& event : events) {
        if (std::holds_alternative<T>(event)) {
            return true;
        }
    }
    return false;
}

inline game::EntityStateDTO TestEntity(game::NetworkEntityId entityId, game::ClientId ownerClientId, game::SnapshotPriority priority)
{
    game::EntityStateDTO entity{};
    entity.movement.entityId = entityId;
    entity.replication.entityId = entityId;
    entity.replication.ownerClientId = ownerClientId;
    entity.replication.priority = priority;
    return entity;
}

} // namespace test_support