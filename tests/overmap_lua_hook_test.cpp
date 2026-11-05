#include "cata_utility.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "init.h"
#include "overmap/overmap.h"
#include "overmap/overmap_lua_hooks.h"
#include "overmap/overmap_special.h"
#include "overmap/overmapbuffer.h"
#include "state_helpers.h"

namespace {

auto lua() -> sol::state& { return DynamicDataLoader::get_instance().lua->lua; } // *NOPAD*

auto ter_at(const point_abs_om& om_pos, const tripoint_om_omt& p) -> std::string {
    return ACTIVE_OVERMAP_BUFFER.get_existing(om_pos)->ter(p).id().str();
}

} // namespace

TEST_CASE("on_overmap_generated edits freshly generated overmaps", "[overmap][lua]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] {
        lua().safe_script("game.hooks.on_overmap_generated = {}");
        clear_all_state();
    });

    // Overmaps earlier cases generated on workers are still queued; flush them first.
    overmap_lua_hooks::run_pending();
    const auto res = lua().safe_script(R"(
        overmap_hook_calls = {}
        game.hooks.on_overmap_generated = {}
        table.insert(game.hooks.on_overmap_generated, function(params)
          local om = params.overmap
          table.insert(overmap_hook_calls, {
            x = params.pos.x, y = params.pos.y, region = params.region, size = om:size(),
          })
          om:ter_set(TripointOmOmt.new(10, 10, 0), OterId.new("forest_thick"))
          om:place_special("Cabin", TripointOmOmt.new(20, 20, 0), 0)
        end)
    )");
    REQUIRE(res.valid());

    const auto om_pos = point_abs_om(3, 4);
    auto specials = overmap_specials::get_default_batch(om_pos);
    ACTIVE_OVERMAP_BUFFER.create_custom_overmap(om_pos, specials);

    const sol::table calls = lua()["overmap_hook_calls"];
    REQUIRE(calls.size() == 1);
    const sol::table call = calls[1];
    CHECK(call.get<int>("x") == 3);
    CHECK(call.get<int>("y") == 4);
    CHECK(call.get<std::string>("region") == "default");
    CHECK(call.get<int>("size") == OMAPX);

    CHECK(ter_at(om_pos, tripoint_om_omt(10, 10, 0)) == "forest_thick");
    CHECK(ter_at(om_pos, tripoint_om_omt(20, 20, 0)) == "cabin_north");
    CHECK(ter_at(om_pos, tripoint_om_omt(20, 20, 1)) == "cabin_roof_north");

    SECTION("existing overmaps do not run the hook again") {
        ACTIVE_OVERMAP_BUFFER.get_om_global(project_combine(om_pos, tripoint_om_omt(1, 1, 0)));
        CHECK(lua().get<sol::table>("overmap_hook_calls").size() == 1);
    }
}
