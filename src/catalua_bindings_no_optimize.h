#pragma once

// Force-included into catalua_bindings*.cpp on MSVC (see src/CMakeLists.txt).
// Equivalent to /Od /Ob0 for these template-heavy TUs, but without overriding
// the configuration's /O2 /Ob1 on the command line (warning D9025).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize( "", off )
#pragma inline_depth( 0 )
#endif
