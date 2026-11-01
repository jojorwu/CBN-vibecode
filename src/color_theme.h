#pragma once

#include <string>
#include <vector>

/// A selectable base color palette shipped in `data/raw/color_themes/`.
struct color_theme {
    /// Human readable name derived from the file name.
    std::string name;
    /// Absolute path to the theme file.
    std::string path;
};

/// All palette themes available in the data directory, sorted by name.
auto available_color_themes() -> std::vector<color_theme>;

/// Copies @p theme over the user's base color palette file.
auto install_color_theme( const color_theme &theme ) -> bool;

/// Reloads the base color palette from disk and redraws with it.
/// @return `false` if the frontend can't apply it without restarting the game.
auto apply_base_color_theme() -> bool;
