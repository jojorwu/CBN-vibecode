#pragma once

#include "coordinates.h"
#include "type_id.h"

/// Runs the Lua `on_overmap_generated` hook for freshly generated overmaps.
///
/// Overmaps are generated on worker threads, but Lua may only run on the main thread,
/// so generation only records the overmap here and the main thread runs the hook
/// once the overmap is in its buffer.
namespace overmap_lua_hooks {

/// Remember a freshly generated (not loaded) overmap. Safe from any thread.
auto mark_generated(const dimension_id& dim, const point_abs_om& pos) -> void;

/// Run the hook for every remembered overmap that is already in its buffer.
/// Main thread only; does nothing when called from a worker thread or re-entrantly.
auto run_pending() -> void;

} // namespace overmap_lua_hooks
