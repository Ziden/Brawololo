#include "GameLogic/Weapons.hpp"

namespace game {

const WeaponDefinition* FindWeaponDefinition(const WeaponDefinitionTable& definitions,
                                             WeaponType type) noexcept {
    for (const auto& definition : definitions) {
        if (definition.type == type) {
            return &definition;
        }
    }

    return nullptr;
}

} // namespace game
