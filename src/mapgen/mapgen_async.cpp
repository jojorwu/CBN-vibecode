#include "mapgen_async.h"

#include "../overmap/overmap_lua_hooks.h"
#include "../overmap/overmapbuffer.h"
#include "auto_note.h"
#include "catalua.h"
#include "color.h"
#include "init.h"
#include "map/map.h"
#include "map/mapbuffer_registry.h"
#include "map_extras.h"
#include "mapgen_constructor.h"
#include "options.h"
#include "overmap/overmapbuffer_registry.h"
#include "point.h"
#include "string_formatter.h"
#include "string_id.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace {

std::mutex g_hook_mutex;
std::vector<deferred_mapgen_hook> g_hooks;

std::mutex g_autonote_mutex;
std::vector<deferred_autonote> g_autonotes;

} // namespace

void push_deferred_mapgen_hook(deferred_mapgen_hook h) {
    // Hooks can be registered at runtime (scripts, tests), so do not gate this on
    // a cached "hooks exist" flag: a stale false would silently drop the hook.
    std::lock_guard<std::mutex> lk(g_hook_mutex);
    g_hooks.push_back(std::move(h));
}

void push_deferred_autonote(deferred_autonote entry) {
    std::lock_guard<std::mutex> lk(g_autonote_mutex);
    g_autonotes.push_back(std::move(entry));
}

void run_deferred_autonotes() {
    std::vector<deferred_autonote> pending;
    {
        std::lock_guard<std::mutex> lk(g_autonote_mutex);
        pending.swap(g_autonotes);
    }
    if (pending.empty()) { return; }

    auto& settings = get_auto_notes_settings();
    const bool notes_active =
        get_option<bool>("AUTO_NOTES") && get_option<bool>("AUTO_NOTES_MAP_EXTRAS");

    std::ranges::for_each(pending, [&](const deferred_autonote& entry) {
        const string_id<map_extra> extra_sid(entry.extra_id);
        settings.set_discovered(extra_sid);

        if (!notes_active || !settings.has_auto_note_enabled(extra_sid)) { return; }

        const map_extra& extra = extra_sid.obj();
        std::string sprite_prefix;
        if (extra.looks_like && !extra.looks_like->empty()) {
            sprite_prefix = "SPRITE:" + *extra.looks_like + ";";
        }
        const std::string mx_note =
            sprite_prefix
            + string_format(
                "%s:%s;<color_yellow>%s</color>: <color_white>%s</color>", extra.get_symbol(),
                get_note_string_from_color(extra.color), extra.name(), extra.description());
        get_overmapbuffer(entry.dim).add_note(entry.omt_pos, mx_note);
    });
}

void run_deferred_mapgen_hooks() {
    // Overmaps generated on workers during mapgen get their Lua hook here, on the main thread.
    overmap_lua_hooks::run_pending();
    // Drain under the lock, then process without holding it.
    std::vector<deferred_mapgen_hook> pending;
    {
        std::lock_guard<std::mutex> lk(g_hook_mutex);
        pending.swap(g_hooks);
    }

    // Fast path: nothing queued.  Skip the Lua state access entirely.
    if (pending.empty()) { return; }

    // Sort by dimension so each batch can reuse one constructor.
    std::ranges::sort(pending, [](const auto& a, const auto& b) { return a.dim < b.dim; });

    // Reuse one constructor per dimension group so the batch function can
    // amortise Lua table allocation and hook-table lookup over all OMTs.
    auto constructor = std::unique_ptr<mapgen_constructor>();
    auto cur_dim = std::optional<dimension_id>();
    std::vector<cata::mapgen_hook_batch_item> batch;
    batch.reserve(pending.size());

    const auto flush = [&]() {
        if (batch.empty()) { return; }
        cata::run_on_mapgen_postprocess_hooks_batch(
            *DynamicDataLoader::get_instance().lua, *constructor, batch);
        batch.clear();
    };

    std::ranges::for_each(pending, [&](const auto& h) {
        if (!cur_dim || h.dim != *cur_dim) {
            flush();
            cur_dim = h.dim;
            constructor = std::make_unique<mapgen_constructor>(MAPBUFFER_REGISTRY.get(h.dim));
        }
        batch.push_back({
            .omt_pos = h.omt_pos,
            .when = h.when,
        });
    });
    flush();
}
