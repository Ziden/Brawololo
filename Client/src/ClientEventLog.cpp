#include "Client/ClientEventLog.hpp"

#include <algorithm>
#include <utility>

namespace game::client {

ClientEventLog::ClientEventLog(std::size_t maxEntries, float entryLifetimeSeconds)
    : maxEntries_(maxEntries), entryLifetimeSeconds_(entryLifetimeSeconds) {}

void ClientEventLog::Push(std::string line) {
    if (line.empty()) {
        return;
    }

    entries_.push_back({std::move(line), entryLifetimeSeconds_});
    while (entries_.size() > maxEntries_) {
        entries_.erase(entries_.begin());
    }
}

void ClientEventLog::PushMany(const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
        Push(line);
    }
}

void ClientEventLog::Update(float deltaSeconds) {
    for (auto& entry : entries_) {
        entry.secondsRemaining -= deltaSeconds;
    }

    const auto expired =
        std::remove_if(entries_.begin(), entries_.end(), [](const ClientEventLogEntry& entry) {
            return entry.secondsRemaining <= 0.0F;
        });
    entries_.erase(expired, entries_.end());
}

std::vector<std::string> ClientEventLog::Lines() const {
    std::vector<std::string> lines{};
    lines.reserve(entries_.size());
    for (const auto& entry : entries_) {
        lines.push_back(entry.line);
    }
    return lines;
}

std::size_t ClientEventLog::Size() const noexcept {
    return entries_.size();
}

} // namespace game::client
