#pragma once

/// Lua-backed enchantment condition. Kept out of enchantment_condition.h so that
/// sol2 is not pulled into every TU that includes character.h / bionics.h.

#include "catalua_sol.h"
#include "enchantment_condition.h"

#include <string>

class enchantment_condition_lua: public virtual enchantment_condition_function {
private:
    std::string name;
    sol::protected_function generic_func;
    sol::protected_function character_func;
    sol::protected_function item_func;
    sol::protected_function item_character_func;

public:
    enchantment_condition_lua(
        const std::string name, sol::protected_function&& generic,
        sol::protected_function&& character, sol::protected_function&& item,
        sol::protected_function&& item_character)
        : name(name),
          generic_func(std::move(generic)),
          character_func(std::move(character)),
          item_func(std::move(item)),
          item_character_func(std::move(item_character)) {}

    virtual auto check_item_condition(const item& it) const -> bool override;
    virtual auto check_item_character_condition(const Character& guy, const item& it) const
        -> bool override;
    virtual auto check_character_condition(const Character& guy, const bool active) const
        -> bool override;
    virtual auto check_generic_condition(const bool active) const -> bool override;
};
