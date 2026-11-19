# Схемы с ручной раскладкой: блоки, панели, стрелки и подписи. Координаты - в пикселях холста,
# (0, 0) - левый верхний угол. Сохраняет .svg (для PDF) и .png (для просмотра в markdown)
import html
import pathlib
import subprocess
import tempfile

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
FONT = "Segoe UI, DejaVu Sans, sans-serif"
INK = "#3a4150"     # линии и стрелки
MUTED = "#4a5163"   # подписи
TEXT = "#1d2330"    # текст в блоках


def text_width(text, size):
    return len(text) * size * 0.55


class Diagram:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.boxes = {}
        self.back, self.lines, self.front = [], [], []

    def panel(self, x, y, w, h, title):
        """Фоновая область с заголовком: x, y - левый верхний угол."""
        self.back.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="12" fill="#f6f7fb" stroke="#c9cfdc" stroke-width="1.2"/>')
        self.back.append(self._text(x + 14, y + 18, title, 14, 600, MUTED, "start"))

    def box(self, key, cx, cy, w, label, h=46, main=False, note=None, external=False):
        """Блок с центром (cx, cy). main - выделенный, note - вторая мелкая строка, external - внешняя система."""
        self.boxes[key] = (cx, cy, w, h)
        fill, stroke, dash = "#e6edfb", "#4a6fb5", ""
        if main:
            fill, stroke = "#d5e1f8", "#2f569c"
        if external:
            fill, stroke, dash = "#ffffff", "#8a93a6", ' stroke-dasharray="6 4"'
        self.front.append(f'<rect x="{cx - w / 2}" y="{cy - h / 2}" width="{w}" height="{h}" rx="8" fill="{fill}" '
                          f'stroke="{stroke}" stroke-width="{2.4 if main else 1.8}"{dash}/>')
        if note:
            self.front.append(self._text(cx, cy - 9, label, 16, 600, TEXT))
            self.front.append(self._text(cx, cy + 12, note, 12.5, 400, MUTED))
        else:
            self.front.append(self._text(cx, cy, label, 16, 700 if main else 600, TEXT))

    def _border(self, key, toward):
        # Точка на границе блока по лучу из его центра в сторону toward
        cx, cy, w, h = self.boxes[key]
        dx, dy = toward[0] - cx, toward[1] - cy
        scale = min(w / 2 / abs(dx) if dx else float("inf"), h / 2 / abs(dy) if dy else float("inf"))
        return cx + dx * scale, cy + dy * scale

    def edge(self, a, b, label=None, at=(0, 0), anchor="middle", dashed=False, both=False, via=None):
        """Стрелка из блока a в блок b. via - точки излома, at - сдвиг подписи от середины линии,
        anchor - чем подпись привязана: start (левым краем), middle, end (правым краем)."""
        points = list(via or [])
        start = self._border(a, points[0] if points else self.boxes[b][:2])
        end = self._border(b, points[-1] if points else self.boxes[a][:2])
        path = [start] + points + [end]
        self.arrow(path, dashed=dashed, both=both)
        if label:
            middle = len(path) // 2
            if len(path) % 2:
                x, y = path[middle]
            else:
                x = (path[middle - 1][0] + path[middle][0]) / 2
                y = (path[middle - 1][1] + path[middle][1]) / 2
            self.label(x + at[0], y + at[1], label, anchor)

    def arrow(self, points, dashed=False, both=False):
        """Стрелка по точкам, без привязки к блокам (например, в край панели)."""
        d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in points)
        dash = ' stroke-dasharray="7 5"' if dashed else ""
        start = ' marker-start="url(#arrow)"' if both else ""
        self.lines.append(f'<path d="{d}" fill="none" stroke="{INK}" stroke-width="1.8"{dash}{start} marker-end="url(#arrow)"/>')

    def label(self, x, y, label, anchor="middle"):
        """Подпись на белой подложке; \\n - перенос строки."""
        lines = label.split("\n")
        width = max(text_width(line, 13) for line in lines) + 10
        height = 17 * len(lines) + 4
        left = {"middle": x - width / 2, "start": x - 5, "end": x - width + 5}[anchor]
        self.front.append(f'<rect x="{left:.1f}" y="{y - height / 2:.1f}" width="{width:.1f}" height="{height}" rx="4" fill="#fff" fill-opacity="0.92"/>')
        for index, line in enumerate(lines):
            self.front.append(self._text(x, y + (index - (len(lines) - 1) / 2) * 17, line, 13, 400, MUTED, anchor))

    @staticmethod
    def _text(x, y, text, size, weight, color, anchor="middle"):
        return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{size}" font-weight="{weight}" '
                f'fill="{color}" text-anchor="{anchor}" dominant-baseline="middle">{html.escape(text)}</text>')

    def save(self, path):
        path = pathlib.Path(path).resolve()
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.width}" height="{self.height}" viewBox="0 0 {self.width} {self.height}">'
               f'<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">'
               f'<path d="M0,0 L10,5 L0,10 z" fill="{INK}"/></marker></defs>'
               f'<rect width="100%" height="100%" fill="#fff"/>' + "".join(self.back + self.lines + self.front) + "</svg>")
        path.write_text(svg, encoding="utf-8")
        # PNG - скриншот той же SVG в Edge с двойной плотностью пикселей
        shot = pathlib.Path(tempfile.mkdtemp()) / "shot.html"
        shot.write_text(f'<!doctype html><html><head><meta charset="utf-8"><style>body{{margin:0;background:#fff}}</style></head><body>{svg}</body></html>', encoding="utf-8")
        subprocess.run([EDGE, "--headless", "--disable-gpu", "--no-first-run", "--hide-scrollbars", "--force-device-scale-factor=2",
                        f"--window-size={self.width + 2},{self.height + 2}", f"--screenshot={path.with_suffix('.png')}", shot.as_uri()],
                       capture_output=True, timeout=120)
        print(path.name, self.width, "x", self.height)
