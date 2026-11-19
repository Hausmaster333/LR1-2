# Перерисовывает схемы и собирает DESIGN.pdf. Запуск из корня лабы: python docs/tools/build.py
# Нужны pandoc и Microsoft Edge (путь к Edge - EDGE в svgkit.py)
import pathlib
import sys

sys.dont_write_bytecode = True  # без __pycache__ в папке лабы

import diagrams  # noqa: E402
import pdf  # noqa: E402

lab = pathlib.Path(__file__).resolve().parents[2]
diagrams.draw(lab / "docs")
pdf.build(lab / "DESIGN.md")
