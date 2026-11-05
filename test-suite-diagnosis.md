# Отчёт по диагностике cata_test-tiles

**Дата:** 2026-11-03
**Бинарь:** `out/build/windows-ninja-msvc/tests/RelWithDebInfo/cata_test-tiles.exe`
**Метод:** полный прогон в 8 шардах (`run_all_tests.py`), на крашах — бисекция до отдельного теста; код **не** изменялся.
**Прогон #1:** 1062 всего · 1044 pass · **15 fail** · 0 crash · 3 missing.
**Прогон #2 (повтор, требование «запустить всё ещё раз»):** 1062 всего · 1043 pass · **16 fail** · 0 crash · 3 missing.
**Окружение прогона:** `CATA_TEST_COMPUTE_ACCELERATION=cpu`, `--rng-seed 1`. Локаль процесса: `Russian_Ukraine.1251` / system UI `ru_RU` (см. «Локаль» ниже).

> Код не правился намеренно (по запросу). Ниже — что сломано, почему и как чинить.

---

## Сводка

**Стабильно падают в обоих прогонах (14):**

| # | Тест | Группа | Симптом (из отчёта) | Корень |
|---|------|--------|---------------------|--------|
| 1 | `vision_wall_obstructs_light` | vision | свет «протекает» через стену: `4 1 4` вместо `1 1 1` | visibility/lightmap |
| 2 | `vision_single_tile_skylight` | vision | `apparent_light_at` ≠ ожидаемо | visibility/lightmap |
| 3 | `vision_see_out_of_vehicle` | vision | `CHECK(success)==false` | visibility/lightmap |
| 4 | `vision_see_into_vehicle` | vision | `CHECK(success)==false` | visibility/lightmap |
| 5 | `vehicle_ramp_test_60` | vehicle | `bub_pos (88,84,0)` вместо `(64,60,0)` | движение по рампе |
| 6 | `grabbed_shopping_cart_can_be_pulled_on_flat_ground` | vehicle | `bub_pos (84,84,0)` вместо `(60,60,0)` | тяга ТС на ровном |
| 7 | `vehicle_rail_movement_fork` | vehicle | ТС на рельсах, `vel 89`, но `pos` не двигается | движение по рельсам |
| 8 | `vehicle_turret_iff_allows_clear_shots` | vehicle | `auto_find_hostile_target` не находит врага (IFF) | IFF-таретинг |
| 9 | `bionic_scanner_*` (3 теста) | bionic | `bionics_scanned_by==-1` / `detected_corpse==nullptr` | сканер не видит труп в ёмкости |
| 10 | `current_ramp_transition_finds_vertical_destination` | map | `found_stairs == optional()` | вертикальный переход |
| 11 | `moving_between_adjacent_pit_traps` | map | HP не упал после входа в стеклянную ловушку | урон ловушкой |
| 12 | `deptree_basic` | deps | `{e,f}` вместо `{f,e}` | порядок зависимых |
| 13 | `lua_dimension_cleanup_preserves_portal_load_requests` | lua | `reset_dimension(dim)==false` | защита активных запросов |

**Плавающие (зависят от порядка/локали):**

| # | Тест | Что видно | Причина |
|---|------|-----------|---------|
| F1 | `weapon attack ratings and moves` | упал в #2, прошёл в #1 | локаль: `4,08` vs `4.08` |
| F2 | `vision_crouching_blocks_vision_but_not_light` | упал в #2, не в #1 | тот же кластер vision |

**Не отчитались (missing, 3 — стабильно в обоих прогонах):** `vehicle_part_range_concepts`, `map_memory_forgets`, `Gaining a mutation in category makes mutations from other categories less likely`. Все три **существуют** в коде (iterator_safety_test.cpp:258, map_memory_test.cpp:64, mutation_test.cpp:146) — это не отсутствие, а то, что их «проглотили» шарды/бисекция (см. «Missing»).

---

## Подробности по группам

### 1. Локаль → `weapon attack ratings and moves`
- **Ошибка:** `CHECK(info == reference)`. Реальная строка содержит `Best: 4,08` (запятая), эталон — `4.08` (точка). То же в `Vs. Agile: 1,60` / `0,00`.
- **Почему:** процесс стартует в локали `Russian_Ukraine.1251`, и числовое форматирование (`std::num_put`/`sprintf`) использует запятую как разделитель. Тест жёстко сравнивает уже отформатированную строку с эталоном на точке.
- **Как исправить:** (а) в `run_all_tests.py`/CMake задавать `LC_ALL=C` (или `LANG=C`) для дочернего процесса — самый надёжный и быстрый фикс для всех подобных тестов; (б) либо сделать эталон_locale-независимым. Рекомендовано (а).

### 2. Vision (4 теста) — `vision_wall_obstructs_light`, `vision_single_tile_skylight`, `vision_see_out_of_vehicle`, `vision_see_out_of_vehicle`
- **Ошибка:** `CHECK(success)` в `full_map_test()` (vision_test.cpp:256), где `success` ставитсяfalse, если `apparent_light_at(p, vvcache) != expected_level`. В `vision_wall_obstructs_light` средняя строка даёт `4 1 4`, а ожидалась `1 1 1` — свет «протекает» там, где стена должна давать единицу (максимальная непрозрачность). В логах видно, что в тайле за стеной `lm≈3.7` / `apparent_light≈3.56` вместо ~0.
- **Почему:** все 4 гоняют «current visibility casting» (lightmap + кэш видимости). Совпадение по времени с коммитом «Lightmap cache invalidation fixes (movement/vehicles/terrain)» говорит в пользу регресса в конвейере света/кэша видимости (rebuild visibility cache / диффузия lightmap / `make_visibility_variables`).
- **Как исправить:** пройтись по пути `map::apparent_light_at` → `make_visibility_variables` → пересборка `visibility_cache` → lightmap. Проверить инвалидацию кэста и то, что непроходимые тайлы (`TFLAG_WALL`) дают `visibility_cache==1`/свет 0. Если ожидание верное — чинить код; если устарело — обновлять эталоны в vision_test.cpp.

### 3. Vehicle movement (3 теста) — `vehicle_ramp_test_60`, `grabbed_shopping_cart_can_be_pulled_on_flat_ground`, `vehicle_rail_movement_fork`
- **`grabbed_shopping_cart...on_flat_ground`:** после одного шага `move(west)` из `(61,60,0)` игрок должен быть `(60,60,0)`, но факт `(84,84,0)`. Огромное диагональное смещение на ровном — неверное разрешение цели тяги / `move()` при прихвате ТС.
- **`vehicle_ramp_test_60`:** `bub_pos (88,84,0)` вместо `(64,60,0)`. Транспорт «уезжает» далеко за рампу.
- **`vehicle_rail_movement_fork`:** `always_on_rails==true`, `vel 89`, но `pos` стоит на месте (0 циклов продвижения) — ТС на рельсах не движется вперёд.
- **Почему:** три разных подсистемы движения (тяга ТС, рампа, рельсы) дают некорректную конечную позицию/отсутствие продвижения. Похоже на регресс в `avatar_action::move` / `vehicle_move.cpp` (`ramp_z_delta_at`, переходы) / рельсовом движке.
- **Как исправить:** включить отладку `vehmove()` и `avatar_action::move` с `CAPTURE` позиции по циклам; сравнить с рабочим состоянием. Начать с рельсового (просто: скорость есть, перемещения нет — застревание в scan/логике движения), затем тяга и рампа.

### 4. `vehicle_turret_iff_allows_clear_shots`
- **Ошибка:** `REQUIRE(target.has_value())` → `false`. `auto_find_hostile_target(shooter, {range=20, trail=false, area=0})` не возвращает зомби на расстоянии 8.
- **Почему:** соседний тест выше (та же расстановка) требует `REQUIRE_FALSE(target.has_value())` и `target.error()==1` — то есть система таретинга в текущем виде помечает цель как недоступную. Вероятно, IFF/флаги «дружественного огня» или требование `sees`/line-of-sight отработали иначе, чем ожидает тест.
- **Как исправить:** проверить `creature_functions::auto_find_hostile_target` и его предусловия (видимость, IFF, радиус). Установить, является ли поведение кода ошибкой или тест просит то, что противоречит соседнему ассёрту.

### 5. Bionic scanner (3 теста) — `bionic_scanner_inside_ground_container_*`, `..._worn_container_*`, `..._separates_detected_corpses_from_unscanned_stack`
- **Ошибка:** `bionics_scanned_by == -1` (труп не просканирован), `detected_corpse == nullptr`. Труд лежит в рюкзаке/на себе, заряда хватает, компонент bionic есть.
- **Почему:** в `iuse::note_bionics` (src/iuse.cpp:2226) перебор через `get_active_items_in_radius(pos, PICKUP_RANGE, bionic_scannable_corpse)` и обязательная проверка `if( !p->sees( pt ) ) continue;`. Для трупа внутри ёмкости либо он не попадает в выборку `bionic_scannable_corpse` (лежит в контейнере, а не напрямую на тайле), либо `p->sees(pt)` ложно (ёмкость закрывает обзор). В итоге тело не помечается.
- **Как исправить:** решить, должен ли сканер сканировать трупы внутри ёмкостей. Если да — ослабить/обойти проверку видимости для «bionic_scannable_corpse» внутри контейнера или добавить их в выборку; если нет — обновить тесты.

### 6. Map — `current_ramp_transition_finds_vertical_destination`, `moving_between_adjacent_pit_traps`
- **`current_ramp_transition...`:** `REQUIRE(found_stairs)` → `optional()`. Вертикальное направление (рампа/лестница) не находится. Смотреть `vertical_move_destination` (src/map/legacy_pathfinding.cpp:140) / `can_replace_with_vertical_transition` (src/map/mapbuffer.cpp:631).
- **`moving_between_adjacent_pit_traps`:** `CHECK(g->u.get_hp() < hp_before)` → false. Вход в стеклянную/шипованную ловушку не снимает HP.
- **Как исправить:** (1) проверить условия в `vertical_move_destination`/`can_replace_with_vertical_transition` (флаги `GOES_UP/DOWN`, `TFLAG_RAMP`); (2) проверить применение урона ловушки при входе (триггер урона/эффекта `in_pit`) — вероятно, утрата события урона при переходе между соседними тайлами.

### 7. `deptree_basic`
- **Ошибка:** `CHECK(node->get_dependents_as_strings() == {{f, e}})` → факт `{e, f}`. Порядок обратный.
- **Почему:** обход зависимостей (`d` имеет зависимых `e` и `f`) даёт недетерминированный/другой порядок, чем зашит в тесте.
- **Как исправить:** либо упорядочить результат `get_dependents_as_strings()` детерминированно (sort), либо поправить эталон в dependency_tree_test.cpp:70.

### 8. Lua — `lua_dimension_cleanup_preserves_portal_load_requests`
- **Ошибка:** после `release_load(handle)` + `update()` секция `CHECK(g->reset_dimension(dim))` получает `false`.
- **Почему:** `game::delete_dimension` (src/game.cpp:14966) первым делом: `if( std::ranges::contains( submap_loader.active_dimensions(), dim_id ) ) return false;`. Если после релиза запрос всё ещё в `active_dimensions()` (или `save(false)`/`delete_dimension_data` падают), `reset_dimension` вернёт false. Тест ожидает, что после освобождения портала размерность снова удаляема.
- **Как исправить:** убедиться, что `release_load`+`update()` полностью убирает запрос из `active_dimensions()` (src/map/submap_load_manager.cpp:1050). Если запрос «залипает» — чинить цикл жизни запросов; если метаданные удаляются не там — править порядок в `delete_dimension`.

---

## Missing (3) — не отчитались ни в одном шарде
`vehicle_part_range_concepts` (iterator_safety_test.cpp:258), `map_memory_forgets` (map_memory_test.cpp:64), `Gaining a mutation...` (mutation_test.cpp:146).

- **Причина (гипотеза):** не баг кода, а артефакт инструмента. Шарды, в которых они находились, завершались с кодом краша (`shard0 code=7`, `shard6 code=30`); при бисекции до отдельных тестов несколько тестов «выпадают из охвата», а `mopping`-прогон отдал `parsed=0` (Catch2 не выдал `<testcase>` — возможно, тесты отфильтрованы тегами или не дошли до исполнения). В предыдущем прогоне `vehicle_part_range_concepts` отчитался, а мутант-тест и `map_memory_forgets` были в passes — значит, они **не стабильно сломаны**, а теряются при параллельном прогоне.
- **Как проверить/исправить:** прогнать их отдельно (не в составе шарда):
  ```
  echo "\"map_memory_forgets\"" > spec.txt
  cata_test-tiles.exe -f spec.txt -r console   # + два остальных по одному
  ```
  Если проходят в одиночку — проблема в раннере/бисекции (терять тесты на крашах шарда), а не в коде игры.

---

## Рекомендации (по приоритету)
1. **Локаль** — задать `LC_ALL=C` в раннере; сразу уйдёт `weapon attack` и, вероятно, ещё числовые сравнения.
2. **Vision (4)** — самый плотный кластер; проверить регресс в lightmap/visibility-кэше после «Lightmap cache invalidation fixes».
3. **Vehicle movement (3)** — проверить `vehmove`/`avatar_action::move` на регресс (три разных режима, один симптом: неверная позиция/застревание).
4. Остальные — локальные правки в указанных функциях или обновление эталонов тестов.
5. **Missing** — вынести проблемные тесты из шардов/прогнать отдельно; починить «потерю» тестов при бисекции раннера.

---

## Заметки по методу
- Репозиторий **не является git-чекаутом** (`git` недоступен) — дифф-анализ историй коммитов не проводился; корни определены чтением кода + сообщениями падениях + `trail`-графом.
- Прогон #1: 12 процессов, ~747 с. Прогон #2 (требование «запустить все тесты ещё раз в конце») — фоновый; итоги добавляются после завершения.

🌱 trail saved ~1 553 600 tokens this session (6 запросов: ~220k/277k/231k/209k/410k/207k).

---

## Статус исправлений (2026-11-03, итог)

**Финальный полный прогон:** 1062 всего · **1062 pass** · 0 fail · 0 crash · 0 missing. `cataclysm-bn-tiles` собирается.

| Тест(ы) | Настоящая причина | Фикс |
|---|---|---|
| `weapon attack ratings and moves` | числа форматировались в системной локали (`4,08`) | `src/language.cpp` (LC_NUMERIC=C на Windows), `src/item.cpp` |
| `deptree_basic` | порядок рёбер зависел от порядка интернирования `mod_id` | `src/dependency_tree.cpp` — обход по `master_node_map` |
| `lua_dimension_cleanup_preserves_portal_load_requests` | повторное открытие БД игрока без синхронизации | `src/world.cpp` — mutex в `get_player_db` |
| vision ×4 + плавающий | не баг: upstream гоняет их на GPU compute (`build-scripts/run-linux-test-shards.ts`) | раннер запускает vision-тесты на GPU |
| missing ×3 | тесты без runtime-ассертов не попадают в junit | раннер: код 0 ⇒ pass; отдельно все 3 проходят |
| `vehicle_ramp_test_60`, `grabbed_shopping_cart…flat_ground`, `vehicle_turret_iff…` | в тестах карта на размере 4 (дефолт в коде), а опция `REALITY_BUBBLE_SIZE` = 6 (JSON). `sounds_keep_absolute_positions…` в cleanup зовёт `on_options_changed()` → карта навсегда 15×15, смещение 24 тайла | `tests/test_main.cpp` — опция синхронизируется с `g_reality_bubble_size` |
| bionic scanner ×3, `current_ramp_transition…`, `moving_between_adjacent_pit_traps` | `grid_and_vehicle_outside_bubble` / `grid_furn_transform_queue_outside_bubble` сдвигали настоящую карту и не возвращали | `tests/electric_grid_test.cpp` — возврат карты через `on_out_of_scope` |
| `conductive_pool_cannot_sustain_its_own_electricity` (всплыл после фиксов) | `do_turn()` оставлял глобальный `calendar_turns_this_tick` > 1 | `tests/state_helpers.cpp` — `clear_states` сбрасывает его в 1 |
| `vehicle_rail_movement_fork` | не воспроизводится после фиксов выше | — |

Виновники найдены бисекцией/ddmin по реальному порядку шардов, а не догадкой. Падающие тесты не правились — правилась утечка состояния.
