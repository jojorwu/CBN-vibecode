#include "afterimage.h"

namespace
{

auto ghosts = std::vector<afterimage::ghost>{};

} // namespace

namespace afterimage
{

auto add( const ghost &g ) -> void { ghosts.push_back( g ); }

auto clear() -> void { ghosts.clear(); }

auto all() -> const std::vector<ghost> & { return ghosts; } // *NOPAD*

} // namespace afterimage
