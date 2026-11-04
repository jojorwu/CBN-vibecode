#include "catch/catch.hpp"
#include "sound_path.h"

#include <string>
#include <vector>

TEST_CASE( "sfx_path_candidates_orders_lookup", "[sound]" )
{
    const auto candidates = sfx_path_candidates( "sound/x.ogg", "data/sound/Basic",
                              "data/mods/foo", "data/" );
    CHECK( candidates == std::vector<std::string>{
        "data/sound/Basic/sound/x.ogg",
        "data/mods/foo/sound/x.ogg",
        "data/sound/x.ogg"
    } );
}

TEST_CASE( "sfx_path_candidates_dedupes_same_directory", "[sound]" )
{
    // Soundpack JSON declares sounds relative to the pack itself, so the
    // source directory must not produce a duplicate candidate.
    const auto candidates = sfx_path_candidates( "menu_move.ogg", "data/sound/Basic",
                              "data/sound/Basic", "data/" );
    CHECK( candidates == std::vector<std::string>{
        "data/sound/Basic/menu_move.ogg",
        "data/menu_move.ogg"
    } );
}

TEST_CASE( "sfx_path_candidates_keeps_absolute_paths", "[sound]" )
{
    const auto candidates = sfx_path_candidates( "/opt/sounds/x.ogg", "data/sound/Basic",
                              "data/mods/foo", "data/" );
    CHECK( candidates == std::vector<std::string>{ "/opt/sounds/x.ogg" } );
}

TEST_CASE( "sfx_path_candidates_without_directories", "[sound]" )
{
    const auto candidates = sfx_path_candidates( "x.ogg", "", "", "" );
    CHECK( candidates == std::vector<std::string>{ "x.ogg" } );
}
