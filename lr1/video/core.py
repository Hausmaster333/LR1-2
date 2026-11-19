"""Движок обучающего видео: слайды на PIL, озвучка через SAPI (Microsoft Irina), сборка ffmpeg.

Видео собирается из шагов. У шага есть текст озвучки и картинка (или короткая анимация);
длительность шага равна длительности его озвучки, поэтому изображение синхронно с речью.
"""
from __future__ import annotations

import hashlib
import json
import math
import os
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import wave
from dataclasses import dataclass, field
from typing import Callable, Optional

from PIL import Image, ImageDraw, ImageFont

W, H = 1920, 1080
ANIM_FPS = 12.5
OUT_FPS = 25
PAD_SECONDS = 0.45  # Пауза после каждой фразы
SAMPLE_RATE = 22050

FONT_DIR = "C:/Windows/Fonts"

BG = (16, 18, 28)
PANEL = (26, 30, 46)
PANEL_2 = (34, 40, 62)
TEXT = (232, 234, 242)
DIM = (140, 147, 172)
BLUE = (92, 150, 255)
ORANGE = (255, 160, 60)
GREEN = (90, 210, 130)
RED = (240, 92, 92)
YELLOW = (240, 200, 70)
PURPLE = (190, 130, 255)
NODE = (48, 78, 140)
NODE_EDGE = (150, 180, 235)


def _font(name: str, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(os.path.join(FONT_DIR, name), size)


class Fonts:
    cache: dict = {}

    @classmethod
    def get(cls, kind: str, size: int) -> ImageFont.FreeTypeFont:
        key = (kind, size)
        if key not in cls.cache:
            name = {"regular": "segoeui.ttf", "bold": "segoeuib.ttf", "mono": "consola.ttf",
                    "mono_bold": "consolab.ttf", "light": "segoeuil.ttf",
                    "symbol": "seguisym.ttf"}[kind]
            cls.cache[key] = _font(name, size)
        return cls.cache[key]


# ---------------------------------------------------------------- рисование

def wrap(text: str, font: ImageFont.FreeTypeFont, width: float) -> list[str]:
    lines: list[str] = []
    for paragraph in text.split("\n"):
        words = paragraph.split(" ")
        line = ""
        for word in words:
            candidate = word if not line else line + " " + word
            if font.getlength(candidate) <= width:
                line = candidate
            else:
                if line:
                    lines.append(line)
                line = word
        lines.append(line)
    return lines


def text_center(draw: ImageDraw.ImageDraw, xy, text, font, fill=TEXT):
    x, y = xy
    width = font.getlength(text)
    box = font.getbbox("Ау")
    height = box[3] - box[1]
    draw.text((x - width / 2, y - height / 2 - box[1]), text, font=font, fill=fill)


def arrow(draw, start, end, fill, width=4, head=16):
    draw.line([start, end], fill=fill, width=width)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    left = (end[0] - head * math.cos(angle - 0.45), end[1] - head * math.sin(angle - 0.45))
    right = (end[0] - head * math.cos(angle + 0.45), end[1] - head * math.sin(angle + 0.45))
    draw.polygon([end, left, right], fill=fill)


def curved_arrow(draw, start, end, fill, bend=0.3, width=4, head=16, arrow_at=0.5):
    dx, dy = end[0] - start[0], end[1] - start[1]
    control = (start[0] + dx / 2 - dy * bend, start[1] + dy / 2 + dx * bend)
    points = []
    for i in range(41):
        t = i / 40
        x = (1 - t) ** 2 * start[0] + 2 * (1 - t) * t * control[0] + t * t * end[0]
        y = (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * control[1] + t * t * end[1]
        points.append((x, y))
    draw.line(points, fill=fill, width=width, joint="curve")
    index = max(1, min(39, int(arrow_at * 40)))
    a, b = points[index - 1], points[index + 1]
    angle = math.atan2(b[1] - a[1], b[0] - a[0])
    tip = (points[index][0] + head * 0.5 * math.cos(angle),
           points[index][1] + head * 0.5 * math.sin(angle))
    left = (tip[0] - head * math.cos(angle - 0.5), tip[1] - head * math.sin(angle - 0.5))
    right = (tip[0] - head * math.cos(angle + 0.5), tip[1] - head * math.sin(angle + 0.5))
    draw.polygon([tip, left, right], fill=fill)


def dashed_line(draw, points, fill, width=3, dash=14, gap=10):
    for (x1, y1), (x2, y2) in zip(points, points[1:]):
        length = math.hypot(x2 - x1, y2 - y1)
        if length == 0:
            continue
        pos = 0.0
        while pos < length:
            end = min(pos + dash, length)
            a = (x1 + (x2 - x1) * pos / length, y1 + (y2 - y1) * pos / length)
            b = (x1 + (x2 - x1) * end / length, y1 + (y2 - y1) * end / length)
            draw.line([a, b], fill=fill, width=width)
            pos += dash + gap


def smooth(p: float) -> float:
    p = max(0.0, min(1.0, p))
    return p * p * (3 - 2 * p)


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


# ---------------------------------------------------------------- деревья
# Дерево - кортеж (key, left, right) или None. Ключ, начинающийся с '#', - это
# поддерево-заглушка (рисуется треугольником), например '#A'.

def node(key, left=None, right=None):
    return (key, left, right)


def bst_insert(tree, key):
    if tree is None:
        return (key, None, None)
    k, l, r = tree
    if key < k:
        return (k, bst_insert(l, key), r)
    if key > k:
        return (k, l, bst_insert(r, key))
    return tree


def bst_from(keys):
    tree = None
    for key in keys:
        tree = bst_insert(tree, key)
    return tree


def height(tree):
    return 0 if tree is None else 1 + max(height(tree[1]), height(tree[2]))


def _rot_left(t):
    k, a, (pk, b, c) = t
    return (pk, (k, a, b), c)


def _rot_right(t):
    k, (pk, a, b), c = t
    return (pk, a, (k, b, c))


def _balance(t):
    k, l, r = t
    bf = height(l) - height(r)
    if bf > 1:
        if height(l[1]) < height(l[2]):
            l = _rot_left(l)
        return _rot_right((k, l, r))
    if bf < -1:
        if height(r[2]) < height(r[1]):
            r = _rot_right(r)
        return _rot_left((k, l, r))
    return t


def avl_insert(tree, key):
    if tree is None:
        return (key, None, None)
    k, l, r = tree
    if key < k:
        tree = (k, avl_insert(l, key), r)
    elif key > k:
        tree = (k, l, avl_insert(r, key))
    else:
        return tree
    return _balance(tree)


def avl_from(keys):
    tree = None
    for key in keys:
        tree = avl_insert(tree, key)
    return tree


def keys_in(tree, order="ЛКП"):
    if tree is None:
        return []
    k, l, r = tree
    parts = {"К": [k], "Л": keys_in(l, order), "П": keys_in(r, order)}
    result = []
    for part in order:
        result += parts[part]
    return result


def layout(tree, box):
    """x - позиция в ЛКП-обходе, y - глубина. Возвращает (позиции, рёбра)."""
    x0, y0, x1, y1 = box
    order = keys_in(tree)
    depth = {}
    edges = []

    def walk(t, d, parent):
        if t is None:
            return
        depth[t[0]] = d
        if parent is not None:
            edges.append((parent, t[0]))
        walk(t[1], d + 1, t[0])
        walk(t[2], d + 1, t[0])

    walk(tree, 0, None)
    n = len(order)
    levels = max(depth.values()) + 1 if depth else 1
    step_x = (x1 - x0) / max(n, 1)
    step_y = min(140, (y1 - y0) / max(levels, 1))
    positions = {}
    for index, key in enumerate(order):
        positions[key] = (x0 + step_x * (index + 0.5), y0 + step_y * (depth[key] + 0.5))
    return positions, edges


def draw_tree(img, tree, box, *, highlight=None, labels=None, positions=None, edges=None,
              radius=34, font_size=30, dim=None, edge_colors=None):
    """highlight: key -> цвет заливки; labels: key -> (текст, цвет) подпись справа сверху."""
    draw = ImageDraw.Draw(img)
    if positions is None:
        positions, edges = layout(tree, box)
    highlight = highlight or {}
    labels = labels or {}
    dim = dim or set()
    edge_colors = edge_colors or {}
    for a, b in edges:
        if a in positions and b in positions:
            color = edge_colors.get((a, b), (95, 105, 140))
            draw.line([positions[a], positions[b]], fill=color, width=4)
    font = Fonts.get("bold", font_size)
    small = Fonts.get("bold", 22)
    for key, (x, y) in positions.items():
        fill = highlight.get(key, NODE)
        if key in dim:
            fill = (40, 44, 60)
        if isinstance(key, str) and key.startswith("#"):
            draw.polygon([(x, y - radius), (x - radius * 1.1, y + radius),
                          (x + radius * 1.1, y + radius)], fill=(60, 66, 92),
                         outline=NODE_EDGE, width=3)
            text_center(draw, (x, y + radius * 0.3), key[1:], font, TEXT)
            continue
        draw.ellipse([x - radius, y - radius, x + radius, y + radius], fill=fill,
                     outline=NODE_EDGE, width=3)
        label = str(key)
        f = font if font.getlength(label) < radius * 1.8 else Fonts.get("bold", font_size - 8)
        text_center(draw, (x, y), label, f, TEXT if key not in dim else DIM)
        if key in labels:
            text, color = labels[key]
            draw.text((x + radius * 0.8, y - radius * 1.55), text, font=small, fill=color)
    return positions


def lerp_positions(a, b, p):
    t = smooth(p)
    result = {}
    for key in b:
        if key in a:
            result[key] = (a[key][0] + (b[key][0] - a[key][0]) * t,
                           a[key][1] + (b[key][1] - a[key][1]) * t)
        else:
            result[key] = b[key]
    return result


# ---------------------------------------------------------------- код и таблицы

KEYWORDS = {"template", "class", "struct", "return", "if", "else", "while", "for", "const",
            "static", "bool", "int", "void", "auto", "nullptr", "true", "false", "std",
            "unique_ptr", "move", "new", "delete", "public", "private", "override", "throw",
            "noexcept", "typename", "continue", "break", "double", "char"}


def draw_code(img, box, code: str, size=27, title=None):
    draw = ImageDraw.Draw(img)
    x0, y0, x1, y1 = box
    draw.rounded_rectangle(box, radius=14, fill=(12, 14, 22), outline=(50, 56, 80), width=2)
    font = Fonts.get("mono", size)
    bold = Fonts.get("mono_bold", size)
    y = y0 + 18
    if title:
        draw.text((x0 + 22, y), title, font=Fonts.get("regular", 22), fill=DIM)
        y += 38
    line_height = size * 1.32
    for line in code.split("\n"):
        x = x0 + 22
        if line.strip().startswith("//"):
            draw.text((x, y), line, font=font, fill=(110, 170, 110))
            y += line_height
            continue
        token = ""
        comment_at = line.find("//")
        code_part = line if comment_at < 0 else line[:comment_at]
        for char in code_part + " ":
            if char.isalnum() or char == "_":
                token += char
                continue
            if token:
                is_kw = token in KEYWORDS
                draw.text((x, y), token, font=bold if is_kw else font,
                          fill=(120, 170, 255) if is_kw else TEXT)
                x += font.getlength(token)
                token = ""
            if char != " " or True:
                color = (200, 200, 215) if char not in "{}()[]<>" else (230, 190, 110)
                draw.text((x, y), char, font=font, fill=color)
                x += font.getlength(char)
        if comment_at >= 0:
            draw.text((x - font.getlength(" "), y), line[comment_at:], font=font,
                      fill=(110, 170, 110))
        y += line_height


def draw_table(img, box, rows, col_widths, size=26, header_color=BLUE, highlight_rows=None):
    draw = ImageDraw.Draw(img)
    x0, y0, x1, _ = box
    font = Fonts.get("regular", size)
    bold = Fonts.get("bold", size)
    row_h = size * 1.75
    highlight_rows = highlight_rows or {}
    y = y0
    for r, row in enumerate(rows):
        fill = PANEL_2 if r == 0 else (PANEL if r % 2 else (22, 25, 38))
        if r in highlight_rows:
            fill = highlight_rows[r]
        draw.rectangle([x0, y, x1, y + row_h], fill=fill)
        x = x0
        for c, cell in enumerate(row):
            color = header_color if r == 0 else TEXT
            draw.text((x + 14, y + row_h * 0.18), str(cell), font=bold if r == 0 else font,
                      fill=color)
            x += col_widths[c]
        y += row_h
    return y


# ---------------------------------------------------------------- слайды

Drawer = Callable[[Image.Image, tuple, float], None]


@dataclass
class Step:
    chapter: str
    title: str
    bullets: list
    say: Optional[str]
    right: Optional[Drawer]
    layout: str
    anim: float  # Длительность анимации, с (0 - статичный кадр)
    silent: float = 0.0  # Для шагов без речи: длительность
    countdown: bool = False
    index: int = 0
    total: int = 1


class Slide:
    def __init__(self, builder, title, layout="split"):
        self.builder = builder
        self.title = title
        self.layout = layout
        self.bullets: list = []
        self.right: Optional[Drawer] = None

    def step(self, say=None, add=None, right=None, anim=0.0, silent=0.0, countdown=False,
             clear=False):
        if clear:
            self.bullets = []
        if add:
            self.bullets += add if isinstance(add, list) else [add]
        if right is not None:
            self.right = right
        self.builder.steps.append(Step(self.builder.chapter, self.title, list(self.bullets), say,
                                       self.right, self.layout, anim, silent, countdown))
        return self


class Builder:
    def __init__(self):
        self.steps: list[Step] = []
        self.chapter = ""

    def set_chapter(self, chapter):
        self.chapter = chapter

    def slide(self, title, layout="split"):
        return Slide(self, title, layout)


def draw_bullets(img, bullets, box, size=34):
    draw = ImageDraw.Draw(img)
    x0, y0, x1, y1 = box
    y = y0
    for raw in bullets:
        level = 0
        text = raw
        while text.startswith("  "):
            level += 1
            text = text[2:]
        color = TEXT
        font_kind = "regular"
        marker = "•" if level == 0 else "–"
        if text.startswith("!"):
            text = text[1:].strip()
            color = ORANGE
            marker = "⚑"
        elif text.startswith("?"):
            text = text[1:].strip()
            color = YELLOW
            marker = "?"
        elif text.startswith("="):
            text = text[1:].strip()
            color = GREEN
            marker = "✓"
        elif text.startswith("#"):
            text = text[1:].strip()
            font_kind = "bold"
            marker = ""
            color = BLUE
        s = size - 4 * level
        font = Fonts.get(font_kind, s)
        indent = x0 + level * 40
        text_x = indent + (36 if marker else 0)
        lines = wrap(text, font, x1 - text_x)
        if marker:
            draw.text((indent, y), marker, font=Fonts.get("symbol", s), fill=color)
        for line in lines:
            draw.text((text_x, y), line, font=font, fill=color)
            y += s * 1.32
        y += s * 0.32
        if y > y1:
            break
    return y


def render_step(step: Step, p: float, countdown_value: Optional[int] = None) -> Image.Image:
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)
    # Шапка
    draw.rectangle([0, 0, W, 6], fill=BLUE)
    draw.text((70, 34), step.chapter, font=Fonts.get("regular", 26), fill=DIM)
    draw.text((70, 72), step.title, font=Fonts.get("bold", 54), fill=TEXT)
    # Прогресс по видео
    progress = (step.index + 1) / max(step.total, 1)
    draw.rectangle([0, H - 8, W, H], fill=(30, 34, 50))
    draw.rectangle([0, H - 8, int(W * progress), H], fill=BLUE)

    if step.layout == "split":
        text_box = (80, 180, 900 if step.right else 1840, 1040)
        draw_bullets(img, step.bullets, text_box)
        if step.right:
            step.right(img, (960, 170, 1860, 1040), p)
    elif step.layout == "full":
        y = draw_bullets(img, step.bullets, (80, 175, 1840, 420), size=32)
        if step.right:
            step.right(img, (80, max(y + 10, 300), 1840, 1050), p)
    elif step.layout == "title":
        if step.right:
            step.right(img, (0, 0, W, H), p)

    if step.countdown:
        value = countdown_value if countdown_value is not None else 0
        draw.rounded_rectangle([W - 380, 40, W - 60, 140], radius=20, fill=(60, 50, 20))
        text_center(draw, (W - 220, 90), f"Пауза... {value}", Fonts.get("bold", 42), YELLOW)
    return img


# ---------------------------------------------------------------- озвучка

TTS_SCRIPT = r"""
param([string]$JobFile)
Add-Type -AssemblyName System.Speech
$jobs = Get-Content -Raw -Encoding UTF8 $JobFile | ConvertFrom-Json
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$s.SelectVoice('Microsoft Irina Desktop')
$s.Rate = 1
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
foreach ($job in $jobs) {
    $s.SetOutputToWaveFile($job.path, $fmt)
    $s.Speak($job.text)
    $s.SetOutputToNull()
}
"""


def synthesize(texts: list[str], cache_dir: str) -> list[str]:
    os.makedirs(cache_dir, exist_ok=True)
    paths, jobs = [], []
    for text in texts:
        digest = hashlib.sha1(text.encode("utf-8")).hexdigest()[:16]
        path = os.path.abspath(os.path.join(cache_dir, digest + ".wav"))
        paths.append(path)
        if not os.path.exists(path) and path not in [j["path"] for j in jobs]:
            jobs.append({"text": text, "path": path})
    if jobs:
        job_file = os.path.abspath(os.path.join(cache_dir, "jobs.json"))
        script_file = os.path.abspath(os.path.join(cache_dir, "tts.ps1"))
        with open(job_file, "w", encoding="utf-8") as f:
            json.dump(jobs, f, ensure_ascii=False)
        with open(script_file, "w", encoding="utf-8-sig") as f:
            f.write(TTS_SCRIPT)
        print(f"TTS: {len(jobs)} фраз...")
        subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                        script_file, "-JobFile", job_file], check=True)
    return paths


# ---------------------------------------------------------------- ElevenLabs

ELEVEN_URL = "https://api.elevenlabs.io/v1"
ELEVEN_MODEL = os.environ.get("ELEVENLABS_MODEL", "eleven_multilingual_v2")


def _eleven_request(path: str, api_key: str, payload: Optional[dict] = None) -> bytes:
    request = urllib.request.Request(
        ELEVEN_URL + path,
        data=None if payload is None else json.dumps(payload).encode("utf-8"),
        headers={"xi-api-key": api_key, "Content-Type": "application/json"},
        method="GET" if payload is None else "POST",
    )
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            body = error.read().decode("utf-8", "replace")
            # 429 (лимит параллельных запросов) и 5xx — временные: ждём и повторяем
            if error.code == 429 or error.code >= 500:
                time.sleep(3 * 2 ** attempt)
                continue
            raise RuntimeError(f"ElevenLabs HTTP {error.code}: {body}") from None
    raise RuntimeError("ElevenLabs: исчерпаны повторы запроса")


def eleven_list_voices(api_key: str):
    voices = json.loads(_eleven_request("/voices", api_key))["voices"]
    for voice in voices:
        labels = ", ".join(f"{k}={v}" for k, v in (voice.get("labels") or {}).items())
        print(f"{voice['voice_id']}  {voice['name']}  {labels}")


def synthesize_eleven(texts: list[str], cache_dir: str, api_key: str, voice_id: str,
                      dry_run: bool = False) -> list[str]:
    """Кэш привязан к модели и голосу; соседние фразы уходят как previous_text/next_text,
    чтобы интонация на стыках кусков была связной."""
    os.makedirs(cache_dir, exist_ok=True)
    paths, missing = [], []
    for index, text in enumerate(texts):
        key = f"{ELEVEN_MODEL}|{voice_id}|{text}"
        digest = hashlib.sha1(key.encode("utf-8")).hexdigest()[:16]
        path = os.path.abspath(os.path.join(cache_dir, "eleven_" + digest + ".wav"))
        paths.append(path)
        if not os.path.exists(path):
            missing.append(index)

    chars = sum(len(texts[i]) for i in missing)
    print(f"ElevenLabs: {len(missing)} фраз, {chars} символов "
          f"(модель {ELEVEN_MODEL}, голос {voice_id})")
    if dry_run:
        raise SystemExit(0)

    query = urllib.parse.urlencode({"output_format": f"pcm_{SAMPLE_RATE}"})
    for number, index in enumerate(missing, 1):
        payload: dict = {
            "text": texts[index],
            "model_id": ELEVEN_MODEL,
            "voice_settings": {"stability": 0.5, "similarity_boost": 0.75},
        }
        if index > 0:
            payload["previous_text"] = texts[index - 1]
        if index + 1 < len(texts):
            payload["next_text"] = texts[index + 1]
        pcm = _eleven_request(f"/text-to-speech/{voice_id}?{query}", api_key, payload)
        # pcm_22050 — сырые 16-битные моно-сэмплы, оборачиваем в WAV как у Irina
        with wave.open(paths[index], "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(SAMPLE_RATE)
            w.writeframes(pcm)
        print(f"  [{number}/{len(missing)}] {len(texts[index])} симв.", flush=True)
    return paths


def wav_seconds(path: str) -> float:
    with wave.open(path, "rb") as w:
        return w.getnframes() / w.getframerate()


# ---------------------------------------------------------------- сборка

def build_video(steps: list[Step], work_dir: str, out_path: str,
                tts: Callable[[list[str], str], list[str]] = synthesize):
    frames_dir = os.path.join(work_dir, "frames")
    os.makedirs(frames_dir, exist_ok=True)
    for old in os.listdir(frames_dir):
        os.remove(os.path.join(frames_dir, old))

    for index, step in enumerate(steps):
        step.index, step.total = index, len(steps)

    spoken = [s.say for s in steps if s.say]
    wavs = iter(tts(spoken, os.path.join(work_dir, "tts")))

    audio = bytearray()
    concat_lines = []
    frame_no = 0
    total_seconds = 0.0

    def emit(img, seconds):
        nonlocal frame_no
        path = os.path.join(frames_dir, f"{frame_no:06d}.png")
        img.save(path, compress_level=1)
        concat_lines.append(f"file '{os.path.abspath(path)}'")
        concat_lines.append(f"duration {seconds:.4f}")
        frame_no += 1

    for index, step in enumerate(steps):
        if step.say:
            wav_path = next(wavs)
            with wave.open(wav_path, "rb") as w:
                data = w.readframes(w.getnframes())
            speech = len(data) / 2 / SAMPLE_RATE
            duration = speech + PAD_SECONDS
            audio += data + b"\x00\x00" * int(PAD_SECONDS * SAMPLE_RATE)
        else:
            duration = step.silent
            audio += b"\x00\x00" * int(duration * SAMPLE_RATE)
        # Длительность кадров строго по числу сэмплов, чтобы звук не уезжал
        duration = (len(audio) / 2 / SAMPLE_RATE) - total_seconds
        total_seconds += duration

        if step.countdown:
            seconds = int(round(duration))
            per = duration / max(seconds, 1)
            for i in range(max(seconds, 1)):
                emit(render_step(step, 1.0, seconds - i), per)
        elif step.anim > 0:
            anim = min(step.anim, duration)
            count = max(2, int(anim * ANIM_FPS))
            for i in range(count):
                emit(render_step(step, i / (count - 1)), anim / count)
            if duration - anim > 0.01:
                emit(render_step(step, 1.0), duration - anim)
        else:
            emit(render_step(step, 1.0), duration)
        print(f"[{index + 1}/{len(steps)}] {step.title}: {duration:.1f} c", flush=True)

    concat_lines.append(concat_lines[-2])  # Требование concat: последний кадр повторить
    concat_path = os.path.join(work_dir, "frames.txt")
    with open(concat_path, "w", encoding="utf-8") as f:
        f.write("\n".join(concat_lines) + "\n")

    audio_path = os.path.join(work_dir, "audio.wav")
    with wave.open(audio_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(bytes(audio))

    print(f"Итого {total_seconds / 60:.1f} мин, кадров {frame_no}. Кодирование...")
    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", concat_path,
        "-i", audio_path, "-fps_mode", "cfr", "-r", str(OUT_FPS), "-c:v", "libx264",
        "-preset", "medium", "-tune", "stillimage", "-crf", "20", "-pix_fmt", "yuv420p",
        "-c:a", "aac", "-b:a", "128k", "-shortest", "-movflags", "+faststart", out_path,
    ], check=True)
    print("Готово:", out_path)
