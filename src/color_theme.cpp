#include "color_theme.h"

#include <algorithm>
#include <ranges>

#include "filesystem.h"
#include "path_info.h"

namespace
{

auto theme_name_from_path( const std::string &path ) -> std::string
{
    using namespace std::views;

    const auto file_name = path.substr( path.find_last_of( "/\\" ) + 1 );
    const auto stem = file_name.substr( 0, file_name.rfind( '.' ) );
    return stem
           | transform( []( const char c ) { return c == '_' ? ' ' : c; } )
           | std::ranges::to<std::string>();
}

auto to_color_theme( const std::string &path ) -> color_theme
{
    return { .name = theme_name_from_path( path ), .path = path };
}

} // namespace

auto available_color_themes() -> std::vector<color_theme>
{
    namespace ranges = std::ranges;
    using namespace std::views;

    const auto dir = PATH_INFO::datadir() + "raw/color_themes/";
    auto themes = get_files_from_path( ".json", dir, false, true )
                  | transform( to_color_theme )
                  | ranges::to<std::vector>();
    ranges::sort( themes, ranges::less{}, &color_theme::name );
    return themes;
}

auto install_color_theme( const color_theme &theme ) -> bool
{
    return copy_file( theme.path, PATH_INFO::base_colors() );
}

#if !defined(TILES)
auto apply_base_color_theme() -> bool { return false; }
#endif
