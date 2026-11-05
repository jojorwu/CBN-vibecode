#include "sound_path.h"

#include <algorithm>
#include <filesystem>

namespace {

auto is_absolute( const std::string &path ) -> bool
{
    if( path.empty() ) {
        return false;
    }
    // Windows treats "\x" as rooted but not absolute; sound paths written with a
    // leading separator are meant literally either way.
    if( path.front() == '/' || path.front() == '\\' ) {
        return true;
    }
    return std::filesystem::path( path ).is_absolute();
}

auto join( const std::string &dir, const std::string &path ) -> std::string
{
    return ( std::filesystem::path( dir ) / path ).generic_string();
}

} // namespace

auto sfx_path_candidates( const std::string &path, const std::string &soundpack_dir,
                          const std::string &source_dir, const std::string &data_dir )
-> std::vector<std::string>
{
    if( is_absolute( path ) ) {
        return { path };
    }

    std::vector<std::string> result;
    for( const auto &dir : { soundpack_dir, source_dir, data_dir } ) {
        if( dir.empty() ) {
            continue;
        }
        auto candidate = join( dir, path );
        if( std::ranges::find( result, candidate ) == result.end() ) {
            result.push_back( std::move( candidate ) );
        }
    }
    if( result.empty() ) {
        result.push_back( path );
    }
    return result;
}
