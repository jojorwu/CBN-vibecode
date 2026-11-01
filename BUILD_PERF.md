# Ускорение сборки и тестов (Cataclysm-BN)

## Что изменено

Влияние (высокое / среднее / низкое) — оценка: на вашей машине сборку не прогоняли.

| # | Изменение | Файлы | Влияние |
| --- | --- | --- | --- |
| 1 | **Убрана утечка `sol/sol.hpp`** (около 1 МБ шаблонов). Раньше он попадал почти везде через `item.h` и `enchantment_condition.h` (→ `bionics.h` → `character.h`). Теперь его тянут **86 из 447** файлов игры (было 329 из 446) и **3 из 193** файлов тестов (было 137 из 191). Lua-условие энчантов вынесено в новый `enchantment_condition_lua.h`, остальные файлы получили прямые include или forward-декларации | `src/item.h`, `src/monstergenerator.h`, `src/recipe_dictionary.h`, `src/catalua.h`, `src/enchantments/*`, 11 `.cpp` | высокое |
| 2 | **PCH на самом деле включён.** Ваш `windows-ninja-msvc` наследовал `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, а `CMakeLists.txt` в этом случае выключает PCH при Ninja (`CATA_USE_PCH`). Получалось, что `build-fast.bat` собирал без PCH, хотя `pch/game-pch.hpp` в репозитории есть. Теперь в пресете `EXPORT_COMPILE_COMMANDS=OFF` и PCH реально используется (и для игры, и для тестов) | `CMakeUserPresets.json` | высокое |
| 3 | **Быстрая линковка MSVC** для Debug и RelWithDebInfo: `/DEBUG:FASTLINK /OPT:NOREF /OPT:NOICF`. Release не трогал. Отключается через `-DCATA_MSVC_FAST_LINK=OFF` (например, если нужен самодостаточный PDB) | `build-scripts/MSVC.cmake` | среднее/высокое для цикла «правка → запуск» |
| 4 | **Unity build теперь работает.** Раньше `set_property(GLOBAL PROPERTY UNITY_BUILD ON)` стоял в конце файла и ни на что не влиял. Теперь `CMAKE_UNITY_BUILD` задаётся до `add_subdirectory(src)`. Пресет `windows-ninja-msvc-unity` — для чистых сборок; для инкрементальных он хуже | `CMakeLists.txt` | среднее (чистая сборка) |
| 5 | В dev-пресетах выключен `LUA_DOCS_ON_BUILD`. Раньше после каждой линковки запускались игра и `deno task docs:gen` | `CMakeUserPresets.json` | среднее |
| 6 | Убран `CMAKE_*_LINKER_LAUNCHER=ccache`: ccache линковку не кэширует, а лишний процесс на каждую линковку стоит времени | `CMakeLists.txt` | низкое |
| 7 | Новый пресет **`linux-dev`** — это `linux-full` без clang-tidy-плагина, без Tracy (который при каждом configure тянул git master), без libbacktrace и lua-docs | `CMakeUserPresets.json` | среднее (WSL) |
| 8 | `ctest` больше не зависит от `sh -c`, поэтому работает и на Windows | `tests/CMakeLists.txt` | — |
| 9 | **`tools/run_tests_parallel.py`** — параллельный запуск тестов, кроссплатформенный (подробности ниже) | новый файл | высокое (время прогона тестов) |

### Как работает `run_tests_parallel.py`

- Catch2 v2 не умеет `--shard-count`, поэтому скрипт делит набор тестов на N процессов сам.
- У каждого процесса свой `--user-dir`.
- Тесты распределяются по шардам по времени прошлого прогона (LPT), чтобы шарды заканчивали одновременно. Тайминги лежат в `.cata_test_times.json`.
- По умолчанию выставляется `CATA_TEST_COMPUTE_ACCELERATION=cpu`, как в CI.

## Как пользоваться

```bat
:: Windows: один раз переконфигурировать (PCH и FASTLINK применятся только после configure)
rmdir /s /q out\build\windows-ninja-msvc
build-fast.bat
cmake --build --preset windows-ninja-game-and-tests

:: Тесты параллельно (из корня репозитория)
python tools\run_tests_parallel.py -j 8
python tools\run_tests_parallel.py "[bionics]"
python tools\run_tests_parallel.py "~[slow]"   :: быстрый прогон
```

```sh
# WSL / Linux
cmake --preset linux-dev && cmake --build --preset linux-dev
python3 tools/run_tests_parallel.py -j "$(nproc)"
```

Проверка ccache: `ccache -s`. Если с PCH пошли промахи, задайте `ccache --set-config sloppiness=pch_defines,time_macros,include_file_mtime`.

## Замеры (в песочнице, clang 18, `-O0`, 1 поток)

На одних и тех же 24 случайных файлах игры:

| Вариант | Время | Ускорение |
| --- | --- | --- |
| Исходное дерево, без PCH (как ваш `build-fast.bat` сейчас) | 122.1 с | — |
| Исправлена утечка `sol.hpp`, без PCH | 106.4 с | ×1.15 |
| Исправлена утечка `sol.hpp` + PCH | 85.7 с | **×1.42** |

Замер сделан на дереве без `game-pch.hpp`: там использовался только `main-pch.hpp`, поэтому в этом репозитории цифры для PCH будут немного лучше. С `-O2` доля фронтенда меньше, поэтому выигрыш в процентах будет скромнее. Быстрая линковка и unity build сюда не входят: их здесь не измерить.

## Что проверено, а что нет

- **Полная компиляция через CMake + Ninja:** 634 объектных файла игры и тестов, включая новые `afterimage.cpp`, изменённый `cata_tiles.cpp`, `sandevistan_test.cpp` и `afterimage_test.cpp`.
  - Скомпилировались 573. Упали 61, но все 61 точно так же падают и на исходном коде: в libstdc++ 13 песочницы нет `std::ranges::to`/`std::expected`, а `catalua_luna.h` использует `std::format` без `#include <format>`. **Регрессий ноль.**
  - Файл с Lua-биндингом, которого это касалось, проверен отдельно с `-include format`: без ошибок.
- **Линковка и запуск тестов здесь невозможны.** Библиотеки SDL3 собрать не дали: сборку стороннего кода заблокировал фильтр безопасности, а пакетные репозитории недоступны. Заголовки SDL использовались только для компиляции. Тесты нужно запустить у вас (команды ниже).
- **MSVC не проверен.** У MSVC STL другие транзитивные include, поэтому где-то может не хватить одного `#include`. PCH с `main-pch.hpp` это в основном прикрывает.
- **Раннер тестов** проверен на тестовом бинарнике с тем же Catch2 v2.13.8.

## Запуск тестов у вас

```bat
cmake --build --preset windows-ninja-game-and-tests
python tools\run_tests_parallel.py -j 8
out\build\windows-ninja-msvc\tests\RelWithDebInfo\cata_test-tiles.exe "[afterimage]"
out\build\windows-ninja-msvc\tests\RelWithDebInfo\cata_test-tiles.exe --mods=sandevistan "[sandevistan]"
out\build\windows-ninja-msvc\src\RelWithDebInfo\cataclysm-bn-tiles.exe --check-mods sandevistan
```
