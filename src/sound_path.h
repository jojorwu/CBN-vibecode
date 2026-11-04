#pragma once

#include <string>
#include <vector>

/**
 * Candidate file system paths for a sound file referenced from JSON, in lookup order.
 *
 * Relative paths are resolved against the active soundpack directory first, then against
 * the directory of the JSON file that declared the sound (content mods keep their audio
 * inside their own folder), and finally against the game data directory (for data-relative
 * paths such as "mods/<mod>/sound/x.ogg").
 *
 * Absolute paths are returned unchanged as the only candidate.
 */
auto sfx_path_candidates( const std::string &path, const std::string &soundpack_dir,
                          const std::string &source_dir, const std::string &data_dir )
-> std::vector<std::string>;
