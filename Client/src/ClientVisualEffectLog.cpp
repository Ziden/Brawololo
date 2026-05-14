#include "Client/ClientVisualEffectLog.hpp"

#include <algorithm>
#include <variant>

namespace game::client {
namespace {

constexpr float LifetimeFor(ViewEffectKind kind) {
    switch (kind) {
        case ViewEffectKind::PredictedFire:
            return 0.35F;
        case ViewEffectKind::AuthoritativeHit:
            return 0.45F;
        case ViewEffectKind::AuthoritativeDeath:
            return 0.8F;
        case ViewEffectKind::AuthoritativeRespawn:
            return 0.7F;
        case ViewEffectKind::PredictionCorrection:
            return 0.4F;
    }

    return 0.35F;
}

} // namespace

ClientVisualEffectLog::ClientVisualEffectLog(std::size_t maxEntries) : maxEntries_(maxEntries) {}

void ClientVisualEffectLog::PushFromEvents(const game::EventList& events,
                                           const ClientRuntime& runtime) {
    const auto localPlayer = runtime.Simulation().PlayerEntityForClient(runtime.LocalClientId());

    for (const auto& event : events) {
        if (const auto* warmup = std::get_if<game::WeaponWarmupStarted>(&event);
            warmup != nullptr && localPlayer.has_value() && warmup->entityId == *localPlayer) {
            if (auto effect =
                    EffectAtEntity(ViewEffectKind::PredictedFire, warmup->entityId, runtime);
                effect.has_value()) {
                Push(*effect, LifetimeFor(effect->kind));
            }
            continue;
        }

        if (const auto* hit = std::get_if<game::HitConfirmed>(&event); hit != nullptr) {
            if (auto effect =
                    EffectAtEntity(ViewEffectKind::AuthoritativeHit, hit->targetId, runtime);
                effect.has_value()) {
                Push(*effect, LifetimeFor(effect->kind));
            }
            continue;
        }

        if (const auto* damaged = std::get_if<game::PlayerDamaged>(&event); damaged != nullptr) {
            if (auto effect = EffectAtEntity(
                    ViewEffectKind::AuthoritativeHit, damaged->entityId, runtime, damaged->damage);
                effect.has_value()) {
                Push(*effect, LifetimeFor(effect->kind));
            }
            continue;
        }

        if (const auto* died = std::get_if<game::PlayerDied>(&event); died != nullptr) {
            if (auto effect =
                    EffectAtEntity(ViewEffectKind::AuthoritativeDeath, died->entityId, runtime);
                effect.has_value()) {
                Push(*effect, LifetimeFor(effect->kind));
            }
            continue;
        }

        if (const auto* respawned = std::get_if<game::PlayerRespawned>(&event);
            respawned != nullptr) {
            ViewEffect effect{};
            effect.kind = ViewEffectKind::AuthoritativeRespawn;
            effect.entityId = respawned->entityId;
            effect.x = respawned->x;
            effect.y = respawned->y;
            Push(effect, LifetimeFor(effect.kind));
            continue;
        }

        if (const auto* corrected = std::get_if<game::LocalPredictionCorrected>(&event);
            corrected != nullptr) {
            ViewEffect effect{};
            effect.kind = ViewEffectKind::PredictionCorrection;
            effect.entityId = corrected->entityId;
            effect.x = corrected->authoritativeX;
            effect.y = corrected->authoritativeY;
            Push(effect, LifetimeFor(effect.kind));
        }
    }
}

void ClientVisualEffectLog::Update(float deltaSeconds) {
    for (auto& entry : entries_) {
        entry.secondsRemaining -= deltaSeconds;
    }

    const auto expired = std::remove_if(
        entries_.begin(), entries_.end(), [](const ClientVisualEffectLogEntry& entry) {
            return entry.secondsRemaining <= 0.0F;
        });
    entries_.erase(expired, entries_.end());
}

std::vector<ViewEffect> ClientVisualEffectLog::Effects() const {
    std::vector<ViewEffect> effects{};
    effects.reserve(entries_.size());

    for (const auto& entry : entries_) {
        auto effect = entry.effect;
        const auto elapsed = entry.lifetimeSeconds - entry.secondsRemaining;
        effect.progressPermille = static_cast<std::uint16_t>(
            std::clamp((elapsed / entry.lifetimeSeconds) * 1000.0F, 0.0F, 1000.0F));
        effects.push_back(effect);
    }

    return effects;
}

std::size_t ClientVisualEffectLog::Size() const noexcept {
    return entries_.size();
}

void ClientVisualEffectLog::Push(ViewEffect effect, float lifetimeSeconds) {
    entries_.push_back(ClientVisualEffectLogEntry{effect, lifetimeSeconds, lifetimeSeconds});
    while (entries_.size() > maxEntries_) {
        entries_.erase(entries_.begin());
    }
}

std::optional<ViewEffect> ClientVisualEffectLog::EffectAtEntity(ViewEffectKind kind,
                                                                game::NetworkEntityId entityId,
                                                                const ClientRuntime& runtime,
                                                                std::int32_t amount) const {
    const auto movement = runtime.Simulation().MovementForEntity(entityId);
    if (!movement.has_value()) {
        return std::nullopt;
    }

    ViewEffect effect{};
    effect.kind = kind;
    effect.entityId = entityId;
    effect.x = movement->x;
    effect.y = movement->y;
    effect.amount = amount;
    return effect;
}

} // namespace game::client
