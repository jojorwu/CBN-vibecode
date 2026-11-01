#include "ui_icons.h"

#include <array>
#include <cstddef>

#include "catacharset.h"

#if defined(TILES)
#include <algorithm>
#include <exception>

#include "debug.h"
#include "path_info.h"
#include "sdltiles.h"
#endif

namespace
{

/// Glyphs present in Terminus, so the fallback stays crisp in both tiles and curses.
constexpr auto fallback_glyphs = std::array{
    "♥", "»", "►", "☺", "◊", "♦", "Ω", "√",
    "Ψ", "≈", "z", "‼", "°", "♪", "⌂", "☼",
    "▒", "¤", "Θ", "≡", "†", "§", "■", "◘",
};
static_assert( fallback_glyphs.size() == static_cast<std::size_t>( ui_icon::count ),
               "every ui_icon needs a fallback glyph" );

#if defined(TILES)

constexpr auto sprite_size = 16;
/// Alpha used for idle (dimmed) icons.
constexpr auto dimmed_alpha = Uint8{ 90 };

struct icon_sheet {
    SDL_Texture_Ptr texture;
    int columns = 0;
    bool attempted = false;
};

auto get_sheet() -> icon_sheet & // *NOPAD*
{
    static auto sheet = icon_sheet{};
    if( sheet.attempted ) {
        return sheet;
    }
    const auto &renderer = get_sdl_renderer();
    if( !renderer ) {
        // No renderer yet (e.g. tests): retry later instead of giving up for good.
        return sheet;
    }
    sheet.attempted = true;

    const auto path = PATH_INFO::gfxdir() + "ui_icons/sidebar_icons.png";
    try {
        const auto surface = load_image( path.c_str() );
        sheet.columns = surface->w / sprite_size;
        sheet.texture = CreateTextureFromSurface( renderer, surface );
    } catch( const std::exception &err ) {
        DebugLog( DL::Warn, DC::SDL ) << "failed to load sidebar icons '" << path << "': "
                                      << err.what();
        sheet.texture.reset();
        return sheet;
    }
    if( sheet.texture ) {
        SDL_SetTextureScaleMode( sheet.texture.get(), SDL_SCALEMODE_NEAREST );
        SDL_SetTextureBlendMode( sheet.texture.get(), SDL_BLENDMODE_BLEND );
    }
    return sheet;
}

#endif // TILES

} // namespace

namespace ui_icons
{

auto fallback_glyph( const ui_icon icon ) -> std::string
{
    return fallback_glyphs[static_cast<std::size_t>( icon )];
}

auto text( const ui_icon icon ) -> std::string
{
#if defined(TILES)
    if( get_sheet().texture ) {
        return utf32_to_utf8( codepoint_base + static_cast<uint32_t>( icon ) );
    }
#endif
    return fallback_glyph( icon ) + " ";
}

auto from_codepoint( const uint32_t codepoint ) -> std::optional<ui_icon>
{
    if( codepoint < codepoint_base ||
        codepoint >= codepoint_base + static_cast<uint32_t>( ui_icon::count ) ) {
        return std::nullopt;
    }
    return static_cast<ui_icon>( codepoint - codepoint_base );
}

#if defined(TILES)
auto draw( const SDL_Renderer_Ptr &renderer, const draw_options &opts ) -> bool
{
    const auto &sheet = get_sheet();
    if( !sheet.texture || sheet.columns <= 0 ) {
        return false;
    }
    const auto index = static_cast<int>( opts.icon );
    const auto src = SDL_FRect{
        static_cast<float>( ( index % sheet.columns ) * sprite_size ),
        static_cast<float>( ( index / sheet.columns ) * sprite_size ),
        static_cast<float>( sprite_size ),
        static_cast<float>( sprite_size )
    };
    const auto side = std::min( opts.area.w, opts.area.h );
    const auto dst = SDL_FRect{
        static_cast<float>( opts.area.x + ( opts.area.w - side ) / 2 ),
        static_cast<float>( opts.area.y + ( opts.area.h - side ) / 2 ),
        static_cast<float>( side ),
        static_cast<float>( side )
    };
    SDL_SetTextureAlphaMod( sheet.texture.get(), opts.dimmed ? dimmed_alpha : Uint8{ 255 } );
    RenderCopy( renderer, sheet.texture, &src, &dst );
    return true;
}
#endif

} // namespace ui_icons
