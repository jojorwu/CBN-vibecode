-- Sandevistan: registration only. Runs before the mod's JSON is loaded.
-- Every callback forwards to `mod.*`, which is defined in main.lua, so hot-reloading
-- main.lua picks up logic changes without re-registering anything.
gdebug.log_info("Sandevistan: preload")

local mod = game.mod_runtime[game.current_mod]

---@param params table
local function on_activate(params) return mod.on_activate(params) end
---@param params table
local function on_deactivate(params) return mod.on_deactivate(params) end
---@param params table
local function on_removed(params) return mod.on_removed(params) end

-- Bionic callbacks are matched to bionics by id (bionic_data::resolve_lua_callbacks).
for _, bionic_id in ipairs({ "bio_sandevistan_mk1", "bio_sandevistan" }) do
  game.bionic_functions[bionic_id] = {
    on_activate = on_activate,
    on_deactivate = on_deactivate,
    on_removed = on_removed,
  }
end

-- Afterimages + per-step energy cost (fires on real steps only).
game.add_hook("on_character_try_move", function(params) return mod.on_character_try_move(params) end)
-- Per-swing energy cost.
game.add_hook("on_creature_melee_attacked", function(params) return mod.on_creature_melee_attacked(params) end)
-- "Time slows": drain the moves other creatures were just granted.
game.add_hook("on_monster_do_turn", function(params) return mod.on_monster_do_turn(params) end)
game.add_hook("on_npc_do_turn", function(params) return mod.on_npc_do_turn(params) end)
game.add_hook("on_game_load", function(params) return mod.on_game_load(params) end)

-- Heat bookkeeping, once per game turn.
gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function() return mod.on_turn() end)
