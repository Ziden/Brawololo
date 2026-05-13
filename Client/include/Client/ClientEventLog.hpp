#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace game::client {

struct ClientEventLogEntry {
    std::string line{};
    float secondsRemaining{};
};

class ClientEventLog {
public:
    explicit ClientEventLog(std::size_t maxEntries = 8, float entryLifetimeSeconds = 3.0F);

    void Push(std::string line);
    void PushMany(const std::vector<std::string>& lines);
    void Update(float deltaSeconds);
    [[nodiscard]] std::vector<std::string> Lines() const;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::size_t maxEntries_{};
    float entryLifetimeSeconds_{};
    std::vector<ClientEventLogEntry> entries_{};
};

} // namespace game::client

