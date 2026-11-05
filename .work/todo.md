# Test suite fixes todo

Baseline (unmodified): 15 fail, 3 missing.
Exe: out/build/windows-ninja-msvc/tests/RelWithDebInfo/cata_test-tiles.exe
Run: CATA_TEST_COMPUTE_ACCELERATION=cpu ; runner: run_all_tests.py

## Failing tests to fix
1. weapon attack ratings and moves — LOCALE (iteminfo formats decimals with Russian C++ locale). FIX: imbue classic locale in iteminfo constructor.
2. vision_wall_obstructs_light — light leaks through wall via cross-z (lm~233 at wall tiles).
3. vision_single_tile_skylight — apparent_light off.
4. vision_see_out_of_vehicle — CHECK(success)==false.
5. vision_see_into_vehicle — CHECK(success)==false.
6. vehicle_ramp_test_60 — bub_pos (88,84,0) vs (64,60,0).
7. grabbed_shopping_cart_can_be_pulled_on_flat_ground — bub_pos (84,84,0) vs (60,60,0).
8. vehicle_rail_movement_fork — vel 89 but pos not moving (PASSING in baseline — recheck).
9. vehicle_turret_iff_allows_clear_shots — auto_find_hostile_target no target.
10. bionic_scanner_* (3) — corpse in container not scanned.
11. current_ramp_transition_finds_vertical_destination — vertical dir not found.
12. moving_between_adjacent_pit_traps — pit damage not applied.
13. deptree_basic — order {e,f} vs {f,e}.
14. lua_dimension_cleanup_preserves_portal_load_requests — reset_dimension false.
15. vision_crouching_blocks_vision_but_not_light — floating (passed baseline).

## Missing (run individually)
- vehicle_part_range_concepts (iterator_safety_test.cpp:258)
- map_memory_forgets (map_memory_test.cpp:64)
- Gaining a mutation in category makes mutations from other categories less likely (mutation_test.cpp:146)
