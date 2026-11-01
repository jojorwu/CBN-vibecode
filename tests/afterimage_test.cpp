#include "afterimage.h"
#include "cata_utility.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "map/map.h"
#include "state_helpers.h"

TEST_CASE("lua_afterimage_binding", "[lua][afterimage]") {
    const auto cleanup = on_out_of_scope([]() {
        afterimage::clear();
        clear_all_state();
    });
    afterimage::clear();
    auto lua = make_lua_state();
    const auto res = lua.safe_script(
        R"(
        local p = TripointBubMs.new(10, 11, 0)
        gapi.add_afterimage({ pos = p, color = "#ff2a2a", alpha = 90 })
        gapi.add_afterimage({ pos = p, color = "#3a6bff" })
        gapi.add_afterimage({ pos = p, color = "#00ff00", alpha = 999 })
    )",
        sol::script_pass_on_error);
    REQUIRE(res.valid());

    const auto& ghosts = afterimage::all();
    REQUIRE(ghosts.size() == 3);
    CHECK(ghosts[0].pos == bub_to_abs(tripoint_bub_ms(10, 11, 0)));
    CHECK(ghosts[0].tint.r == 0xff);
    CHECK(ghosts[0].tint.g == 0x2a);
    CHECK(ghosts[0].alpha == 90);
    CHECK(ghosts[1].tint.b == 0xff);
    CHECK(ghosts[1].alpha == 160); // default
    CHECK(ghosts[2].alpha == 255); // clamped

    const auto bad = lua.safe_script(
        R"(gapi.add_afterimage({ pos = TripointBubMs.new(0, 0, 0), color = "not a color" }))",
        sol::script_pass_on_error);
    CHECK_FALSE(bad.valid());
    CHECK(afterimage::all().size() == 3);

    REQUIRE(lua.safe_script("gapi.clear_afterimages()", sol::script_pass_on_error).valid());
    CHECK(afterimage::all().empty());
}
