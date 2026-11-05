# Отчёт о прогоне тестов — 2026-11-05

## Метод

- Бинарник: `out/build/windows-ninja-msvc/tests/RelWithDebInfo/cata_test-tiles.exe`
- Запуск из корня проекта: `python tools/run_tests_parallel.py -j 8 -- -r compact`
- 1063 теста, 8 шардов, wall time **535.8 с**
- Окружение раннера: `CATA_TEST_COMPUTE_ACCELERATION=cpu`, `--rng-seed time`, локаль `Russian_Ukraine.1251`
- Полный лог: `out/test-run-full.log`
- Код/тесты **не исправлялись** (по заданию).

## Итог

- **6 упавших тестов, 19 проваленных assertions, 0 крашей.**
- Шарды 1 и 2 прошли полностью. Упали шарды 0, 3, 4, 5, 6, 7.
- Отдельно `unskilled_shooter_accuracy` (`[!mayfail]`) дал 4 провала, но раннер их не считает — это разрешённое падение.
- Первый прогон (до перезапуска с полным логом) уронил только 3 шарда — результаты **нестабильны между прогонами**.

## Упавшие тесты

| # | Тест | Шард | Провалов | Сообщение | В изоляции |
|---|------|------|----------|-----------|------------|
| 1 | `lua_examine_dimension_travel_reenters_mapgen_and_save_hooks` | 0 | 2 | `tests/catalua_test.cpp(1430): mapgen_calls > 0` → `0 > 0` (секции `reset` и `delete`) | **падает всегда** (cpu и gpu) |
| 2 | `vision_single_tile_skylight` | 3 | 8 | `tests/vision_test.cpp(256): success` → false, все 8 трансформаций | падает только на `cpu_compute`; на GPU-бэкенде проходит |
| 3 | `fixed window activity skip completes a short wait with active creatures` | 4 | 3 | `activity_fixed_window_test.cpp(111/112/115)`: время не сдвинулось на `duration`, активность осталась, таймер `49` вместо `20` | проходит → зависит от порядка/состояния |
| 4 | `vision_see_out_of_vehicle` | 5 | 1 | `vision_test.cpp(256): success` → false | только на `cpu_compute` |
| 5 | `vision_see_into_vehicle` | 6 | 1 | `vision_test.cpp(256): success` → false | только на `cpu_compute` |
| 6 | `vision_wall_obstructs_light` | 7 | 4 | `vision_test.cpp(256)`: трансформации 0–3, свет «протекает» сквозь стену (`4` вместо `1`) | только на `cpu_compute` |
| — | `unskilled_shooter_accuracy` `[!mayfail]` | 5 | 4 | `ranged_balance_test.cpp(154/191)`: `good_stats.avg()` 0.12–0.14 при пороге 0.5; `fast_stats.avg()` ниже `hit_rate` | падает всегда, но помечен как разрешённый |

Значения в сообщениях печатаются с запятой (`0,14`) — следствие системной локали, на суть не влияет.

## Причины

1. **Vision (4 теста) — бэкенд раннера, а не регресс кода.**
   `run_tests_parallel.py` по умолчанию выставляет `CATA_TEST_COMPUTE_ACCELERATION=cpu`
   (строка 107). На `cpu_compute` тесты видят неверную освещённость/видимость.
   Без этой переменной (бэкенд `sdl_gpu_software`) все четыре vision-теста проходят.
   Тот же вывод был в `test-suite-diagnosis.md` от 2026-11-03.

2. **Lua-тест — единственное воспроизводимое падение.**
   Падает и на CPU, и на GPU в одиночном прогоне. Тест существует в HEAD (не из
   локального диффа). Наиболее вероятная связь — незакоммиченные изменения
   `src/map/map.cpp` + `src/map/submap_load_manager.cpp`: параллельная предзагрузка
   OMT перед `loadn()` и вызов `on_mapgen_postprocess` через отложенные хуки.
   Это гипотеза, бисекция не делалась.

3. **`fixed window activity skip...` — загрязнение состояния.**
   Изолированно проходит на обоих бэкендах; в шарде падает. Похоже на утечку
   глобального состояния между тестами (время/активность/опции).

4. **`unskilled_shooter_accuracy`** помечен `[!mayfail]`, поэтому Catch2 не валит
   прогон; менять ничего не нужно, если падение ожидаемо.

## Проблема инструмента

Первый прогон упал сам при печати лога:
`run_tests_parallel.py:151` → `UnicodeEncodeError: 'charmap' codec can't encode...`
(консоль cp1251). Из-за этого потерялись результаты шардов 5–7.
Обход без правки скрипта: `PYTHONIOENCODING=utf-8` + копия скрипта с полным выводом.

## Trail

Использован `trail ask` (граф обновлён): найдены `tools/run_tests_parallel.py`,
`map::apply_vision_transparency_cache`, `Character::get_vision_threshold`,
`test_crossing_threshold`.
