#pragma once
#include <array>
#include <cstddef>

namespace UtilitySuite::Protocol {
// Wire positions shared with phantomui.assist.AssistOptions. Append; never reorder.
enum class Option : std::size_t {
    AUTO_ARMOR = 0,
    REFILL = 1,
    HIT_EFFECT = 2,
    PREDICT_DOUBLE_HAND = 3,
    SHIELD_BREAKER = 4,
    ANTI_BOT = 5,
    ARMOR_DELAY = 6,
    KEEP_ELYTRA = 7,
    REFILL_DELAY = 8,
    REFILL_THRESHOLD = 9,
    PARTICLE_COUNT = 10,
    PARTICLE_LIFETIME = 11,
    PREDICT_MELEE = 12,
    PREDICT_CRYSTAL = 13,
    PREDICT_ANCHOR = 14,
    DANGER_RADIUS = 15,
    DAMAGE_MARGIN = 16,
    RETURN_SLOT = 17,
    SHIELD_AUTOMATIC = 18,
    SHIELD_DELAY = 19,
    AUTO_TOTEM = 20,
    ARMOR_LOGS = 21,
    PREDICT_LOGS = 22,
    HIT_LOGS = 23,
    CONFIRMED_ONLY = 24,
    PARTICLE_SPREAD = 25,
    COUNT = 26
};
inline constexpr std::size_t Count = static_cast<std::size_t>(Option::COUNT);
constexpr std::size_t Index(Option option) { return static_cast<std::size_t>(option); }
template<class Value> using Payload = std::array<Value, Count>;
}
