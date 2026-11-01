// Integration tests for the `sandevistan` Lua+JSON mod (data/mods/sandevistan).
// They drive the real bionic code paths and the mod's real Lua callbacks and hooks.
// Hidden by default ([.]) because the mod must be loaded. Run with:
//   cata_test-tiles --mods=sandevistan "[sandevistan]"

#include "avatar.h"
#include "bionics.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catalua.h"
#include "catalua_coord.h"
#include "catalua_hooks.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "init.h"
#include "map_helpers.h"
#include "monster.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units.h"

#include <string>

namespace {

const auto bio_sandevistan = bionic_id("bio_sandevistan");
const auto effect_sande_vision = efftype_id("sande_vision");
const auto effect_sande_cooldown = efftype_id("sande_cooldown");
const auto heat_key = std::string("sandevistan_heat");

auto mod_loaded() -> bool {
    if (bio_sandevistan.is_valid()) { return true; }
    WARN("sandevistan mod not loaded, skipping; run with --mods=sandevistan");
    return false;
}

auto heat_of(const avatar& who) -> double {
    const auto raw = who.get_value(heat_key);
    return raw.empty() ? 0.0 : std::stod(raw);
}

/// One game turn of Lua bookkeeping, as game::do_turn runs it.
auto advance_turn() -> void {
    calendar::turn += 1_turns;
    cata::run_on_every_x_hooks(*DynamicDataLoader::get_instance().lua);
}

auto reset_avatar(avatar& who) -> void {
    who.remove_value(heat_key);
    who.set_max_power_level(1000_kJ);
    who.set_power_level(1000_kJ);
}

auto cleanup(avatar& who) -> void {
    if (who.has_active_bionic(bio_sandevistan)) {
        who.deactivate_bionic(who.get_bionic_state(bio_sandevistan));
    }
    who.remove_value(heat_key);
    clear_all_state();
}

} // namespace

TEST_CASE("sandevistan activation grants vision effect and speed", "[.][sandevistan][lua]") {
    if (!mod_loaded()) { return; }
    clear_all_state();
    auto& dummy = get_avatar();
    const auto restore = on_out_of_scope([&]() { cleanup(dummy); });
    reset_avatar(dummy);

    dummy.reset();
    const auto speed_before = dummy.get_speed();
    REQUIRE(speed_before > 0);

    // activate_bionic recalculates the enchantment cache (applying ench_effects) and reset()s.
    give_and_activate_bionic(dummy, bio_sandevistan);
    CHECK(dummy.has_effect(effect_sande_vision));
    // SPEED multiply 3.0 adds 3x base on top of base (about 4x total).
    CHECK(dummy.get_speed() >= speed_before * 3);

    dummy.deactivate_bionic(dummy.get_bionic_state(bio_sandevistan));
    CHECK_FALSE(dummy.has_effect(effect_sande_vision));
    CHECK(dummy.get_speed() == speed_before);
}

TEST_CASE("sandevistan heat builds per turn and overheats", "[.][sandevistan][lua]") {
    if (!mod_loaded()) { return; }
    clear_all_state();
    auto& dummy = get_avatar();
    const auto restore = on_out_of_scope([&]() { cleanup(dummy); });
    reset_avatar(dummy);

    give_and_activate_bionic(dummy, bio_sandevistan);
    REQUIRE(heat_of(dummy) == Approx(0.0));

    advance_turn();
    CHECK(heat_of(dummy) == Approx(8.0));

    // 8 heat per turn: 96 after 12 turns, still running.
    for (auto i = 1; i < 12; ++i) { advance_turn(); }
    CHECK(heat_of(dummy) == Approx(96.0));
    REQUIRE(dummy.has_active_bionic(bio_sandevistan));
    CHECK_FALSE(dummy.has_effect(effect_sande_cooldown));

    // Turn 13 crosses 100: the Lua side force-deactivates the bionic.
    advance_turn();
    CHECK_FALSE(dummy.has_active_bionic(bio_sandevistan));
    CHECK(dummy.has_effect(effect_sande_cooldown));
    CHECK_FALSE(dummy.has_effect(effect_sande_vision));
    CHECK(heat_of(dummy) == Approx(100.0));
}

TEST_CASE("sandevistan slows monsters through on_monster_do_turn", "[.][sandevistan][lua]") {
    if (!mod_loaded()) { return; }
    clear_all_state();
    auto& dummy = get_avatar();
    const auto restore = on_out_of_scope([&]() { cleanup(dummy); });
    reset_avatar(dummy);

    auto& zombie = spawn_test_monster("mon_zombie", dummy.bub_pos() + tripoint_rel_ms(2, 0, 0));
    // Mirrors the call in game::monmove (src/game.cpp).
    const auto run_monster_hook = [&]() {
        REQUIRE(cata::has_hooks("on_monster_do_turn"));
        cata::run_hooks("on_monster_do_turn", [&zombie](sol::table& params) {
            params["monster"] = &zombie;
        });
    };

    give_and_activate_bionic(dummy, bio_sandevistan);
    // The mod takes back 90% of the creature's per-turn move gain (debt may go negative).
    zombie.set_moves(100);
    run_monster_hook();
    CHECK(zombie.get_moves() == 100 - static_cast<int>(zombie.get_speed() * 9 / 10));

    dummy.deactivate_bionic(dummy.get_bionic_state(bio_sandevistan));
    zombie.set_moves(100);
    run_monster_hook();
    CHECK(zombie.get_moves() == 100);
}

TEST_CASE("sandevistan charges energy per step and shuts down when dry", "[.][sandevistan][lua]") {
    if (!mod_loaded()) { return; }
    clear_all_state();
    auto& dummy = get_avatar();
    const auto restore = on_out_of_scope([&]() { cleanup(dummy); });
    reset_avatar(dummy);
    // Mirrors the call in game::walk_move (src/game.cpp).
    const auto run_step_hook = [&]() {
        cata::run_hooks("on_character_try_move", [&dummy](sol::table& params) {
            params["char"] = static_cast<Character*>(&dummy);
            params["from"] = cata::detail::lua_coords::to_lua(dummy.bub_pos());
            params["to"] = cata::detail::lua_coords::to_lua(dummy.bub_pos() + tripoint_rel_ms(1, 0, 0));
        });
    };

    give_and_activate_bionic(dummy, bio_sandevistan);
    const auto before = dummy.get_power_level();
    run_step_hook();
    // Military unit: 5 kJ per step on top of the engine's per-turn react_cost.
    CHECK(dummy.get_power_level() == before - 5_kJ);

    dummy.set_power_level(4_kJ);
    run_step_hook();
    CHECK_FALSE(dummy.has_active_bionic(bio_sandevistan));
    CHECK(dummy.get_power_level() == 4_kJ);
}
