#pragma once

#include "coordinates.h"
#include "hsv_color.h"

#include <cstdint>
#include <vector>

/// Translucent tinted copies of the avatar drawn by the tiles renderer
/// (Sandevistan-style afterimages). Driven from Lua via gapi.add_afterimage.
namespace afterimage
{

struct ghost {
    tripoint_abs_ms pos;
    RGBColor tint;
    uint8_t alpha = 160;
};

auto add( const ghost &g ) -> void;
auto clear() -> void;
auto all() -> const std::vector<ghost> &; // *NOPAD*

} // namespace afterimage
