#include "catch/catch.hpp"
#include "catacharset.h"
#include "panels.h"
#include "ui_icons.h"

#include <algorithm>
#include <cstdint>

TEST_CASE( "icons_sidebar_layout_is_default", "[panels]" )
{
    auto manager = panel_manager{};
    CHECK( manager.get_current_layout_id() == "icons" );
    CHECK( is_narrow_layout( manager.get_current_layout_id() ) );
}

TEST_CASE( "icons_sidebar_layout_fits_narrow_width", "[panels]" )
{
    namespace ranges = std::ranges;
    auto manager = panel_manager{};
    const auto &layout = manager.get_current_layout();
    REQUIRE_FALSE( layout.empty() );
    CHECK( ranges::all_of( layout, []( const auto & panel ) { return panel.get_width() == 32; } ) );
    const auto is_status = []( const auto & panel ) { return panel.get_name() == "Status"; };
    CHECK( ranges::any_of( layout, is_status ) );
}

TEST_CASE( "narrow_layout_detection", "[panels]" )
{
    CHECK( is_narrow_layout( "compact" ) );
    CHECK( is_narrow_layout( "labels-narrow" ) );
    CHECK( is_narrow_layout( "icons" ) );
    CHECK_FALSE( is_narrow_layout( "labels" ) );
    CHECK_FALSE( is_narrow_layout( "classic" ) );
}

TEST_CASE( "ui_icon_markers_round_trip", "[panels][ui_icons]" )
{
    const auto count = static_cast<uint32_t>( ui_icon::count );
    for( auto i = uint32_t{ 0 }; i < count; ++i ) {
        const auto icon = static_cast<ui_icon>( i );
        const auto marker = utf32_to_utf8( ui_icons::codepoint_base + i );
        CHECK( utf8_width( marker ) == ui_icons::cell_width );
        CHECK( ui_icons::from_codepoint( UTF8_getch( marker ) ) == icon );
        CHECK( utf8_width( ui_icons::fallback_glyph( icon ) ) == 1 );
    }
    CHECK_FALSE( ui_icons::from_codepoint( ui_icons::codepoint_base + count ).has_value() );
    CHECK_FALSE( ui_icons::from_codepoint( 'A' ).has_value() );
}

TEST_CASE( "ui_icon_text_is_two_cells_wide", "[panels][ui_icons]" )
{
    CHECK( utf8_width( ui_icons::text( ui_icon::health ) ) == ui_icons::cell_width );
    CHECK( utf8_width( ui_icons::text( ui_icon::volume ) ) == ui_icons::cell_width );
}