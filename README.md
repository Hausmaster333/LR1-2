# ЛР‑1 и ЛР‑2

ЛР‑1 демонстрирует AVL-дерево, обходы и прошивку. ЛР‑2 использует это же дерево для виртуальной файловой системы и поискового индекса. Общее ядро находится в `shared/include`, его тесты в `shared/tests`. Код приложений, дизайны и результаты замеров лежат в `lr1` и `lr2`.

## Сборка

Выполняйте команды из каталога с этим Makefile. Требуются GNU Make, g++ с поддержкой C++17, GLFW и OpenGL. Для Ubuntu/WSL пакеты сборки — `build-essential libglfw3-dev libgl1-mesa-dev libx11-dev`. Для Windows используйте MinGW-w64 и GLFW из одного окружения MSYS2 UCRT64.

| Действие | Windows (PowerShell) | Linux / WSL |
|---|---|---|
| Собрать обе лабораторные | `mingw32-make` | `make` |
| Собрать ЛР‑1 полностью | `mingw32-make LAB=1` | `make LAB=1` |
| Собрать GUI ЛР‑2 | `mingw32-make LAB=2 gui` | `make LAB=2 gui` |
| Собрать и запустить тесты ЛР‑2 | `mingw32-make LAB=2 test` | `make LAB=2 test` |
| Собрать бенчмарк ЛР‑1 | `mingw32-make LAB=1 benchmark` | `make LAB=1 benchmark` |

Для каждой цели можно указать `LAB=1` или `LAB=2`. Без параметров собираются GUI, тестовый бинарник и бенчмарк обеих работ.

## Запуск

Из корня проекта запустите `.\build\windows\lr1\gui.exe` или `./build/linux/lr1/gui`. Для проводника замените `lr1` на `lr2`. Для графических приложений в WSL нужна графическая сессия.

Короткий замер ЛР‑2 на Windows — `.\build\windows\lr2\benchmark.exe --max 1000`, на Linux — `./build/linux/lr2/benchmark --max 1000`. Для ЛР‑1 замените номер каталога в команде. По умолчанию бенчмарк обновляет `lr1/benchmarks/results.csv` или `lr2/benchmarks/results.csv`; графики соответствующего GUI читают этот же файл. Чтобы сохранить предыдущие измерения, задайте другой путь через `--out`.
