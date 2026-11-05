#include "overmap_lua_hooks.h"

#include "catalua_coord.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "regional_settings.h"
#include "thread_pool.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

struct pending_overmap {
    dimension_id dim;
    point_abs_om pos;
};

std::mutex pending_mutex;
std::vector<pending_overmap> pending;
// Main thread only: a hook that generates more overmaps must not run hooks recursively.
bool running = false;

auto run_hook(const pending_overmap& entry, overmap& om) -> void {
    cata::run_hooks("on_overmap_generated", [&](sol::table& params) {
        params["overmap"] = &om;
        params["pos"] = cata::detail::lua_coords::to_lua(entry.pos);
        params["region"] = om.get_settings().id;
        params["dimension"] = entry.dim.str();
    });
}

} // namespace

auto overmap_lua_hooks::mark_generated(const dimension_id& dim, const point_abs_om& pos) -> void {
    const auto lock = std::lock_guard(pending_mutex);
    pending.push_back({.dim = dim, .pos = pos});
}

auto overmap_lua_hooks::run_pending() -> void {
    if (running || is_pool_worker_thread()) { return; }
    running = true;
    // Hooks may generate further overmaps; keep draining until nothing new is ready.
    while (true) {
        auto batch = std::vector<pending_overmap>();
        {
            const auto lock = std::lock_guard(pending_mutex);
            batch.swap(pending);
        }
        auto waiting = std::vector<pending_overmap>();
        auto ran_any = false;
        std::ranges::for_each(batch, [&](const pending_overmap& entry) {
            // Still being inserted by overmapbuffer::generate(); retry on a later drain.
            auto* const om = get_overmapbuffer(entry.dim).get_existing(entry.pos);
            if (om == nullptr) {
                waiting.push_back(entry);
                return;
            }
            if (cata::has_hooks("on_overmap_generated")) { run_hook(entry, *om); }
            ran_any = true;
        });
        {
            const auto lock = std::lock_guard(pending_mutex);
            pending.insert(pending.end(), waiting.begin(), waiting.end());
        }
        if (!ran_any) { break; }
    }
    running = false;
}
