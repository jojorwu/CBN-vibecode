#include "../src/map/map.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "monster.h"
#include "options_helpers.h"
#include "rng.h"
#include "sampling_profiler.h"
#include "state_helpers.h"
#include "type_id.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>

namespace {

struct turn_benchmark_options {
    int monster_count;
    int item_piles;
    int turns;
};

/// Walls in a sparse grid so vision, light and pathfinding have obstacles to work around.
auto scatter_walls() -> void {
    auto& here = get_map();
    for (int x = 2; x < MAPSIZE_X - 2; x += 9) {
        for (int y = 2; y < MAPSIZE_Y - 2; y += 9) {
            for (int i = 0; i < 4; ++i) { here.ter_set(tripoint_bub_ms(x + i, y, 0), ter_id("t_wall")); }
        }
    }
}

auto scatter_items(const int piles) -> void {
    auto& here = get_map();
    static const auto item_types = std::array{"rock", "2x4", "jeans", "can_beans", "water_clean"};
    for (int i = 0; i < piles; ++i) {
        const auto p = tripoint_bub_ms(rng(5, MAPSIZE_X - 6), rng(5, MAPSIZE_Y - 6), 0);
        if (here.impassable(p)) { continue; }
        for (const auto* type : item_types) { here.add_item_or_charges(p, item::spawn(itype_id(type))); }
    }
}

auto spawn_horde(const tripoint_bub_ms& center, const int count) -> void {
    auto& here = get_map();
    auto spawned = 0;
    while (spawned < count) {
        const auto p = center + tripoint(rng(-30, 30), rng(-30, 30), 0);
        if (square_dist(p, center) < 8 || here.impassable(p) || g->critter_at(p) != nullptr) { continue; }
        spawn_test_monster("mon_zombie", p);
        ++spawned;
    }
}

auto run_turn_benchmark(const turn_benchmark_options& opts) -> void {
    const auto no_autosave = override_option("AUTOSAVE", "false");
    clear_all_state();
    rng_set_engine_seed(12345);
    calendar::turn = calendar::turn_zero + 12_hours;
    build_test_map(ter_id("t_pavement"));
    scatter_walls();
    scatter_items(opts.item_piles);

    auto& u = get_avatar();
    const auto center = tripoint_bub_ms(MAPSIZE_X / 2, MAPSIZE_Y / 2, 0);
    u.setpos(center);
    get_map().ter_set(center, ter_id("t_pavement"));
    spawn_horde(center, opts.monster_count);
    g->reset_light_level();
    get_map().invalidate_map_cache(0);
    get_map().build_map_cache(0, true);

    auto bubble_resizes = 0;
    auto last_mapsize = g_mapsize;
    auto profiler = sampling_profiler();
    const auto started = std::chrono::steady_clock::now();
    profiler.start();
    for (int t = 0; t < opts.turns; ++t) {
        // Keep the player passive and alive so do_turn never waits for input.
        u.set_moves(-1'000'000);
        u.set_all_parts_hp_to_max();
        REQUIRE_FALSE(g->do_turn());
        if (g_mapsize != last_mapsize) {
            ++bubble_resizes;
            last_mapsize = g_mapsize;
        }
    }
    profiler.stop();
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started);

    const auto summary = std::format(
        "turn benchmark: {} monsters, {} item piles, {} turns: {:.1f} ms total, {:.3f} ms/turn, "
        "{} bubble resizes\n",
        opts.monster_count, opts.item_piles, opts.turns, elapsed.count(), elapsed.count() / opts.turns,
        bubble_resizes);
    const auto report = summary + profiler.report(60);
    std::fputs(report.c_str(), stdout);
    std::ofstream("turn_perf_report.txt", std::ios::app) << report << "\n";
    clear_all_state();
}

} // namespace

TEST_CASE("turn_performance_benchmark", "[.][benchmark][turn_perf]") {
    // Several runs, because single-run timings vary by tens of percent.
    for (int run = 0; run < 5; ++run) {
        run_turn_benchmark({.monster_count = 80, .item_piles = 200, .turns = 300});
    }
}
