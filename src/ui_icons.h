#pragma once

#include <cstdint>
#include <optional>
#include <string>

#if defined(TILES)
#include "sdl_wrappers.h"
#endif

/// Sidebar icons, in sprite sheet order (see tools/gen_sidebar_icons.py).
enum class ui_icon : int {
    health,
    stamina,
    speed,
    mood,
    focus,
    power,
    mana,
    safe,
    hunger,
    thirst,
    rest,
    pain,
    heat,
    sound,
    place,
    sky,
    underground,
    light,
    time,
    date,
    weapon,
    style,
    weight,
    volume,
    count
};

namespace ui_icons
{

/// First codepoint of the plane 15 private use range that marks icon cells.
constexpr auto codepoint_base = uint32_t{ 0xF0000 };
/// Last codepoint of the reserved marker range.
constexpr auto codepoint_last = uint32_t{ 0xF00FF };
/// Display cells taken by `text()`, whether it yields a sprite marker or a font glyph.
constexpr auto cell_width = 2;

/// Text that draws `icon` in exactly `cell_width` cells.
/// In tiles builds with the sprite sheet loaded this is a marker that the renderer replaces
/// with the sprite; otherwise it is a font glyph followed by a space.
auto text( ui_icon icon ) -> std::string;

/// Font glyph used when the sprite sheet is unavailable.
auto fallback_glyph( ui_icon icon ) -> std::string;

/// Icon encoded by a marker codepoint, if any.
auto from_codepoint( uint32_t codepoint ) -> std::optional<ui_icon>;

#if defined(TILES)
struct draw_options {
    ui_icon icon = ui_icon::health;
    SDL_Rect area;
    bool dimmed = false;
};

/// Draws the sprite centered in `area`, scaled to the largest square that fits.
/// Returns false when the sprite sheet is not loaded.
auto draw( const SDL_Renderer_Ptr &renderer, const draw_options &opts ) -> bool;
#endif

} // namespace ui_icons
