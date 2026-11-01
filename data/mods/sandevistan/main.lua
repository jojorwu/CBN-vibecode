-- Sandevistan: runtime logic. Everything here is safe to hot-reload; the engine
-- callbacks registered in preload.lua only forward to the `mod.*` functions below.
--
-- Mechanics overview
--   * Engaging the bionic (JSON enchantments) multiplies the avatar's speed and turns
--     the view green; this file adds everything that needs logic:
--   * world slow-down: other creatures only gain `world_factor` of their speed in moves
--     per turn, so they act that fraction of the time;
--   * rainbow afterimages trailing the avatar (engine ghosts in tiles, '@' fields in ASCII);
--   * energy: the engine charges act_cost on activation and react_cost every turn, and
--     this file additionally charges ACTION_COST for every step and melee swing, because
--     a boosted avatar performs several actions per game turn;
--   * heat: builds every engaged turn; at 100% the unit force-shuts down (nosebleed,
--     stun, pain, cooldown). Heat bleeds off while the unit is idle.
gdebug.log_info("Sandevistan: main")

local mod = game.mod_runtime[game.current_mod]

---@class SandeTier
---@field id string             bionic id
---@field label string          short name for the sidebar
---@field heat_per_turn number  heat gained per game turn while engaged
---@field world_factor number   fraction of their speed other creatures still get in moves per turn
---@field action_cost integer   extra energy (J) per step / melee swing while engaged
---@field trail_len integer     how many afterimages stay on the map

---@type SandeTier[]
local TIERS = {
  {
    id = "bio_sandevistan",
    label = "MIL",
    heat_per_turn = 8,
    world_factor = 0.1,
    action_cost = 5000,
    trail_len = 10,
  },
  {
    id = "bio_sandevistan_mk1",
    label = "Mk.1",
    heat_per_turn = 5,
    world_factor = 0.5,
    action_cost = 2000,
    trail_len = 6,
  },
}

local HEAT_MAX = 100
local HEAT_REFUSE = 90 -- won't engage at or above this heat
local COOL_PER_TURN = 3
local OVERHEAT_COOLDOWN_TURNS = 90

local TRAIL_COLORS = 7
-- Same hues as fd_sande_trail_1..7 in fields.json: red, orange, yellow, green, cyan, blue, violet.
local TRAIL_HEX = { "#ff2a2a", "#ff8c1a", "#ffe81a", "#2aff6a", "#1affff", "#3a6bff", "#c43aff" }
local GHOST_ALPHA_MIN = 50 -- oldest afterimage
local GHOST_ALPHA_MAX = 220 -- newest afterimage

local KEY_HEAT = "sandevistan_heat" -- per-character value, saved with the game

local EFF_HEAT = EffectTypeId.new("sande_heat")
local EFF_COOLDOWN = EffectTypeId.new("sande_cooldown")
local EFF_BLEED = EffectTypeId.new("bleed")
local EFF_STUNNED = EffectTypeId.new("stunned")
local BP_HEAD = BodyPartTypeId.new("head")

-- Runtime-only state, rebuilt on game load. Leftover fields fade via their half_life.
mod.trail = mod.trail or {}
mod.trail_step = mod.trail_step or 0
mod.active = mod.active or nil ---@type SandeTier|nil
mod.trail_fids = nil -- resolved lazily: field int ids only exist after data load

-- ---------------------------------------------------------------------------
-- Small helpers

---@param id string
---@return SandeTier|nil
local function tier_by_id(id)
  for _, t in ipairs(TIERS) do
    if t.id == id then return t end
  end
  return nil
end

--- The tier whose bionic is currently switched on, if any.
---@param who Character
---@return SandeTier|nil
local function active_tier(who)
  for _, t in ipairs(TIERS) do
    if who:has_active_bionic(BionicDataId.new(t.id)) then return t end
  end
  return nil
end

--- The tier whose bionic is installed (on or off), if any.
---@param who Character
---@return SandeTier|nil
local function installed_tier(who)
  for _, t in ipairs(TIERS) do
    if who:has_bionic(BionicDataId.new(t.id)) then return t end
  end
  return nil
end

---@param who Character
---@return number
local function get_heat(who) return tonumber(who:get_value(KEY_HEAT)) or 0 end

---@param who Character
---@param value number
local function set_heat(who, value) who:set_value(KEY_HEAT, string.format("%.4f", value)) end

---@param v number
---@param lo number
---@param hi number
---@return number
local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

---@param who Character
---@return integer joules
local function power_j(who) return who:get_power_level():to_joule() end

-- ---------------------------------------------------------------------------
-- Afterimages
--
-- Trail entries keep ABSOLUTE positions: the local (bub) grid shifts whenever the
-- avatar crosses a submap edge, which happens constantly while sprinting.

---@return any[] field int ids for fd_sande_trail_1..7
local function trail_fids()
  if not mod.trail_fids then
    mod.trail_fids = {}
    for i = 1, TRAIL_COLORS do
      mod.trail_fids[i] = FieldTypeId.new("fd_sande_trail_" .. i):int_id()
    end
  end
  return mod.trail_fids
end

--- Tiles + an engine with gapi.add_afterimage: translucent tinted copies of the
--- avatar (the anime look). Otherwise (ASCII, or an unpatched engine): '@' fields.
---@return boolean
local function use_ghosts() return gapi.afterimages_supported ~= nil and gapi.afterimages_supported() end

--- Re-submit every ghost so older ones fade out (alpha falls with age).
local function render_ghosts()
  gapi.clear_afterimages()
  local n = #mod.trail
  for i, ghost in ipairs(mod.trail) do
    local alpha = math.floor(GHOST_ALPHA_MIN + (GHOST_ALPHA_MAX - GHOST_ALPHA_MIN) * i / n)
    gapi.add_afterimage({ pos = gapi.abs_to_bub(ghost.abs), color = TRAIL_HEX[ghost.hue], alpha = alpha })
  end
end

---@param ghost table trail entry
local function remove_field_of(ghost)
  if ghost.fid then gapi.get_map():remove_field_at(gapi.abs_to_bub(ghost.abs), ghost.fid) end
end

local function clear_trail()
  if gapi.clear_afterimages then gapi.clear_afterimages() end
  for _, ghost in ipairs(mod.trail) do
    remove_field_of(ghost)
  end
  mod.trail = {}
end

--- Leave an afterimage at `pos`; the hue advances one step around the rainbow.
---@param pos any TripointBubMs
---@param max_len integer
local function drop_afterimage(pos, max_len)
  mod.trail_step = (mod.trail_step % TRAIL_COLORS) + 1
  local ghost = { abs = gapi.bub_to_abs(pos), hue = mod.trail_step }
  if use_ghosts() then
    table.insert(mod.trail, ghost)
  else
    ghost.fid = trail_fids()[mod.trail_step]
    if gapi.get_map():add_field_at(pos, ghost.fid, 1, TimeDuration.from_turns(0)) then table.insert(mod.trail, ghost) end
  end
  while #mod.trail > max_len do
    remove_field_of(table.remove(mod.trail, 1))
  end
  if use_ghosts() then render_ghosts() end
end

-- ---------------------------------------------------------------------------
-- Sound, heat effect

--- Sounds ship inside the mod; the engine patch (the same one that adds
--- gapi.add_afterimage) teaches the sound loader to find mod-relative files.
---@param variant string "activate" | "deactivate" | "overheat"
local function play_sound(variant)
  if gapi.afterimages_supported ~= nil then gapi.play_variant_sound("sandevistan", variant, 100, false) end
end

---@param heat number
---@return integer 0..3
local function heat_level(heat)
  if heat >= 90 then return 3 end
  if heat >= 75 then return 2 end
  if heat >= 50 then return 1 end
  return 0
end

--- Keep the "sande_heat" status effect at the intensity matching `heat`.
---@param who Character
---@param heat number
local function sync_heat_effect(who, heat)
  local want = heat_level(heat)
  local current = who:has_effect(EFF_HEAT) and who:get_effect_int(EFF_HEAT) or 0
  if current ~= want and current > 0 then who:remove_effect(EFF_HEAT) end
  if want > 0 then who:add_effect(EFF_HEAT, TimeDuration.from_turns(5), nil, want) end
end

-- ---------------------------------------------------------------------------
-- Shutdowns. `mod.shutdown_reason` tells on_deactivate which message/sound to use.

--- Involuntary switch-off. eff_only=true: no deact_cost and no "You deactivate"
--- message (the Lua binding names this argument block_message, but it is eff_only).
---@param who Character
---@param tier SandeTier
---@param reason string
---@return boolean false if the engine refused (e.g. the bionic is EMP-incapacitated)
local function force_off(who, tier, reason)
  mod.shutdown_reason = reason
  local ok = who:deactivate_bionic(BionicDataId.new(tier.id), true)
  mod.shutdown_reason = nil
  return ok
end

---@param who Character
---@param tier SandeTier
local function overheat(who, tier)
  -- If the engine refuses, retry next turn instead of stacking penalties.
  if not force_off(who, tier, "overheat") then return end
  play_sound("overheat")
  gapi.add_msg(MsgType.bad, "<color_light_red>SANDEVISTAN OVERHEAT.</color>  The world slams back to full speed.")
  gapi.add_msg(MsgType.bad, "Blood runs from your nose and the floor tilts under you.")
  who:add_effect(EFF_BLEED, TimeDuration.from_minutes(3), BP_HEAD, 1)
  who:add_effect(EFF_STUNNED, TimeDuration.from_turns(gapi.rng(2, 5)))
  who:add_effect(EFF_COOLDOWN, TimeDuration.from_turns(OVERHEAT_COOLDOWN_TURNS))
  who:mod_pain(15)
  set_heat(who, HEAT_MAX)
end

---@param who Character
---@param tier SandeTier
local function out_of_power(who, tier)
  if not force_off(who, tier, "power") then return end
  play_sound("deactivate")
  gapi.add_msg(MsgType.bad, "Your Sandevistan runs out of power and the world snaps back to speed.")
end

--- Charge the per-action energy cost; shuts the unit down if the avatar can't pay.
---@param who Character
local function charge_action(who)
  local tier = mod.active
  if not tier then return end
  if power_j(who) < tier.action_cost then
    out_of_power(who, tier)
    return
  end
  who:mod_power_level(Energy.from_joule(-tier.action_cost))
end

-- ---------------------------------------------------------------------------
-- Bionic callbacks (registered in preload.lua)

---@param params { user: Character, bionic: any }
mod.on_activate = function(params)
  local who = params.user
  local tier = tier_by_id(params.bionic:id():str())
  if not tier then return end

  local heat = get_heat(who)
  if who:has_effect(EFF_COOLDOWN) or heat >= HEAT_REFUSE then
    if who:is_avatar() then
      gapi.add_msg(MsgType.warning, string.format("The Sandevistan refuses to engage: spine at %d%%.", math.floor(heat)))
    end
    force_off(who, tier, "refused")
    -- The engine charged act_cost before calling us (bionics.cpp: power is spent, then
    -- the Lua callback runs); refund it, nothing happened.
    who:mod_power_level(params.bionic:info().power_activate)
    return
  end

  if not who:is_avatar() then
    -- Heat is only tracked for the avatar; an NPC could otherwise run it forever.
    force_off(who, tier, "refused")
    who:mod_power_level(params.bionic:info().power_activate)
    return
  end
  mod.active = tier
  clear_trail()
  drop_afterimage(who:get_pos_ms(), tier.trail_len)
  play_sound("activate")
  gapi.add_msg(MsgType.good, "<color_light_green>Sandevistan engaged.</color>  Everything around you slows to a crawl.")
end

---@param params { user: Character, bionic: any }
mod.on_deactivate = function(params)
  local who = params.user
  if not who:is_avatar() then return end
  mod.active = nil
  clear_trail()
  if mod.shutdown_reason == nil then -- a manual switch-off
    play_sound("deactivate")
    gapi.add_msg(MsgType.info, "You disengage the Sandevistan.  The world lurches back up to speed.")
  end
end

---@param params { user: Character, bionic_id: any }
mod.on_removed = function(params)
  local who = params.user
  if who:has_effect(EFF_HEAT) then who:remove_effect(EFF_HEAT) end
  if who:is_avatar() then
    mod.active = nil
    clear_trail()
  end
end

-- ---------------------------------------------------------------------------
-- Game hooks (registered in preload.lua)

--- Fires from game::walk_move, i.e. only for real steps (not for attacks). It runs
--- before the last checks (e.g. the dangerous-terrain prompt), so a step cancelled
--- there still leaves an afterimage and costs energy; acceptable for a rare case.
---@param params { char: Character, from: any, to: any }
---@return boolean always true: never blocks movement
mod.on_character_try_move = function(params)
  local who = params.char
  if mod.active and who and who:is_avatar() then
    drop_afterimage(params.from, mod.active.trail_len)
    charge_action(who)
  end
  return true
end

---@param params { char: Character, target: Creature, success: boolean }
mod.on_creature_melee_attacked = function(params)
  local who = params.char
  if mod.active and who and who:is_avatar() then charge_action(who) end
end

--- "Time slows": take back (1 - world_factor) of the creature's per-turn move gain.
--- Subtracting a fixed amount (even into negative moves) lets the debt build up, so the
--- creature really acts only world_factor of the time. Scaling the current moves would
--- not work: a creature acts whenever moves > 0. Also independent of whether the hook
--- runs before (NPCs) or after (monsters) the turn's moves are granted.
---@param critter Creature
local function slow_down(critter)
  local tier = mod.active
  if not tier or critter:is_avatar() then return end
  critter:mod_moves(-math.floor(critter:get_speed() * (1 - tier.world_factor)))
end

---@param params { monster: Monster }
mod.on_monster_do_turn = function(params) slow_down(params.monster) end

---@param params { npc: Npc }
mod.on_npc_do_turn = function(params) slow_down(params.npc) end

mod.on_game_load = function()
  if gapi.clear_afterimages then gapi.clear_afterimages() end
  mod.trail = {}
  mod.trail_fids = nil
  mod.active = active_tier(gapi.get_avatar())
end

--- Once per game turn: heat up while engaged, cool down while idle.
mod.on_turn = function()
  local who = gapi.get_avatar()
  if not who then return end
  local heat = get_heat(who)
  if not installed_tier(who) and heat <= 0 then return end

  local tier = active_tier(who)
  mod.active = tier
  if tier then
    heat = heat + tier.heat_per_turn
    set_heat(who, heat)
    if heat >= HEAT_MAX then
      overheat(who, tier)
      heat = HEAT_MAX
    end
  else
    heat = math.max(0, heat - COOL_PER_TURN)
    set_heat(who, heat)
  end
  sync_heat_effect(who, heat)
end

-- ---------------------------------------------------------------------------
-- Sidebar widget: state, heat gauge, power gauge

---@param ratio number 0..1
---@param width integer
---@return string
local function bar(ratio, width)
  local filled = math.floor(clamp(ratio, 0, 1) * width + 0.5)
  return string.rep("#", filled) .. string.rep("-", width - filled)
end

---@param width integer
---@param _height integer
---@return table[]
local function draw_widget(width, _height)
  local who = gapi.get_avatar()
  if not who then return {} end
  local tier = installed_tier(who)
  if not tier then return {} end

  local heat = get_heat(who)
  local state, state_color
  if mod.active then
    state, state_color = "ENGAGED", "light_green"
  elseif who:has_effect(EFF_COOLDOWN) then
    state, state_color = "COOLING", "light_red"
  else
    state, state_color = "ready", "light_gray"
  end
  local heat_color = ({ [0] = "light_gray", "yellow", "light_red", "red" })[heat_level(heat)]
  local power = power_j(who)
  local max_power = math.max(1, who:get_max_power_level():to_joule())
  local actions_left = math.floor(power / tier.action_cost)
  local power_color = actions_left < 10 and "light_red" or "light_cyan"
  local bar_w = math.max(5, math.min(16, width - 18))

  return {
    { text = string.format("Sandevistan %s: %s", tier.label, state), color = state_color },
    { text = string.format("Heat  [%s] %3d%%", bar(heat / HEAT_MAX, bar_w), math.floor(heat)), color = heat_color },
    {
      text = string.format("Power [%s] %4d kJ", bar(power / max_power, bar_w), math.floor(power / 1000)),
      color = power_color,
    },
  }
end

-- Re-registering replaces the widget by id, so hot reload picks up a new draw function.
sidebar.register_widget({
  id = "sandevistan_status",
  name = "Sandevistan",
  height = 3,
  order = 26,
  default_toggle = true,
  panel_visible = function()
    local who = gapi.get_avatar()
    return who ~= nil and installed_tier(who) ~= nil
  end,
  draw = function(w, h) return draw_widget(w, h) end,
})
