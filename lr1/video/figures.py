"""Фабрики иллюстраций: каждая возвращает функцию draw(img, box, p), p - прогресс анимации."""
from __future__ import annotations

import csv
import math
import os

from PIL import ImageDraw

from core import (BLUE, DIM, GREEN, NODE, ORANGE, PANEL, PURPLE, RED, TEXT, YELLOW, Fonts,
                  arrow, curved_arrow, dashed_line, draw_code, draw_table, draw_tree, keys_in,
                  layout, lerp_positions, mix, smooth, text_center, wrap)


def caption(img, box, text, color=DIM, size=26):
    draw = ImageDraw.Draw(img)
    x0, _, x1, y1 = box
    text_center(draw, ((x0 + x1) / 2, y1 - 24), text, Fonts.get("regular", size), color)


def tree_fig(tree, highlight=None, labels=None, note=None, dim=None, radius=34,
             edge_colors=None):
    def draw(img, box, p):
        x0, y0, x1, y1 = box
        inner = (x0, y0 + 10, x1, y1 - (60 if note else 10))
        draw_tree(img, tree, inner, highlight=highlight, labels=labels, dim=dim, radius=radius,
                  edge_colors=edge_colors)
        if note:
            caption(img, box, note)
    return draw


def two_trees_fig(left, right, left_note, right_note, left_hl=None, right_hl=None,
                  arrow_text=None, labels_left=None, labels_right=None):
    def draw(img, box, p):
        x0, y0, x1, y1 = box
        mid = (x0 + x1) / 2
        lbox = (x0, y0 + 10, mid - 40, y1 - 70)
        rbox = (mid + 40, y0 + 10, x1, y1 - 70)
        draw_tree(img, left, lbox, highlight=left_hl, labels=labels_left, radius=30)
        draw_tree(img, right, rbox, highlight=right_hl, labels=labels_right, radius=30)
        caption(img, (x0, y0, mid - 40, y1), left_note)
        caption(img, (mid + 40, y0, x1, y1), right_note)
        d = ImageDraw.Draw(img)
        arrow(d, (mid - 30, (y0 + y1) / 2), (mid + 30, (y0 + y1) / 2), ORANGE, 5, 20)
        if arrow_text:
            text_center(d, (mid, (y0 + y1) / 2 - 40), arrow_text, Fonts.get("bold", 24), ORANGE)
    return draw


def morph_fig(before, after, note=None, highlight=None, labels_before=None, labels_after=None,
              radius=34):
    """Плавный переход между двумя деревьями с теми же ключами (повороты)."""
    def draw(img, box, p):
        x0, y0, x1, y1 = box
        inner = (x0, y0 + 10, x1, y1 - (60 if note else 10))
        a_pos, a_edges = layout(before, inner)
        b_pos, b_edges = layout(after, inner)
        t = smooth((p - 0.15) / 0.7)
        positions = lerp_positions(a_pos, b_pos, t)
        edges = a_edges if t < 0.5 else b_edges
        labels = labels_before if t < 0.5 else labels_after
        draw_tree(img, None, inner, highlight=highlight, labels=labels, positions=positions,
                  edges=edges, radius=radius)
        if note:
            caption(img, box, note)
    return draw


def traversal_fig(tree, order, color=ORANGE):
    seq = keys_in(tree, order)

    def draw(img, box, p):
        x0, y0, x1, y1 = box
        inner = (x0, y0 + 10, x1, y1 - 120)
        shown = int(round(p * len(seq)))
        visited = seq[:shown]
        highlight = {k: color for k in visited}
        if shown < len(seq):
            highlight[seq[shown]] = mix(NODE, color, 0.35)
        labels = {k: (str(i + 1), YELLOW) for i, k in enumerate(visited)}
        draw_tree(img, tree, inner, highlight=highlight, labels=labels)
        d = ImageDraw.Draw(img)
        text = order + ":  " + "  ".join(str(k) for k in visited)
        d.rounded_rectangle([x0, y1 - 100, x1, y1 - 20], radius=14, fill=PANEL)
        d.text((x0 + 24, y1 - 86), text, font=Fonts.get("mono_bold", 38), fill=TEXT)
    return draw


def thread_fig(tree, order, note=None):
    seq = keys_in(tree, order)

    def draw(img, box, p):
        x0, y0, x1, y1 = box
        inner = (x0, y0 + 10, x1, y1 - (70 if note else 10))
        positions = draw_tree(img, tree, inner, highlight={seq[0]: GREEN})
        d = ImageDraw.Draw(img)
        shown = int(round(p * (len(seq) - 1)))
        for a, b in list(zip(seq, seq[1:]))[:shown]:
            pa, pb = positions[a], positions[b]
            curved_arrow(d, pa, pb, ORANGE, bend=0.25, width=4, head=18)
        # Узлы поверх нитей, чтобы подписи читались
        draw_tree(img, tree, inner, highlight={seq[0]: GREEN}, positions=positions,
                  edges=[])
        if note:
            caption(img, box, note)
    return draw


def code_fig(code, title=None, size=27):
    def draw(img, box, p):
        draw_code(img, box, code, size=size, title=title)
    return draw


def table_fig(rows, widths, size=27, highlight_rows=None):
    def draw(img, box, p):
        x0, y0, x1, y1 = box
        draw_table(img, (x0, y0 + 10, x0 + sum(widths), y1), rows, widths, size=size,
                   highlight_rows=highlight_rows)
    return draw


def big_text_fig(lines, size=44, color=TEXT):
    """lines: список строк или (строка, цвет); выводится по центру области."""
    def draw(img, box, p):
        d = ImageDraw.Draw(img)
        x0, y0, x1, y1 = box
        items = [(l, color) if isinstance(l, str) else l for l in lines]
        total = len(items) * size * 1.6
        y = (y0 + y1) / 2 - total / 2 + size * 0.8
        for text, c in items:
            text_center(d, ((x0 + x1) / 2, y), text, Fonts.get("bold", size), c)
            y += size * 1.6
    return draw


def stack_fig(*drawers, ratios=None):
    """Несколько иллюстраций друг под другом."""
    ratios = ratios or [1] * len(drawers)

    def draw(img, box, p):
        x0, y0, x1, y1 = box
        total = sum(ratios)
        y = y0
        for drawer, ratio in zip(drawers, ratios):
            height = (y1 - y0) * ratio / total
            drawer(img, (x0, y, x1, y + height), p)
            y += height
    return draw


def side_fig(*drawers, ratios=None):
    ratios = ratios or [1] * len(drawers)

    def draw(img, box, p):
        x0, y0, x1, y1 = box
        total = sum(ratios)
        x = x0
        for drawer, ratio in zip(drawers, ratios):
            width = (x1 - x0) * ratio / total
            drawer(img, (x + 10, y0, x + width - 10, y1), p)
            x += width
    return draw


def serialize(tree, lits=("{", "}(", ")[", "]"), order="КЛП"):
    """Токены сериализации: (текст, вид), вид - 'lit' или 'val'."""
    if tree is None:
        return []
    k, l, r = tree
    parts = {"К": [(str(k), "val")], "Л": serialize(l, lits, order),
             "П": serialize(r, lits, order)}
    tokens = [(lits[0], "lit")]
    for i, part in enumerate(order):
        tokens += parts[part]
        tokens.append((lits[i + 1], "lit"))
    return tokens


def string_fig(tree, lits=("{", "}(", ")[", "]"), order="КЛП", note=None):
    tokens = serialize(tree, lits, order)

    def draw(img, box, p):
        x0, y0, x1, y1 = box
        tree_box = (x0, y0, x1, y0 + (y1 - y0) * 0.55)
        draw_tree(img, tree, tree_box, radius=30)
        d = ImageDraw.Draw(img)
        font = Fonts.get("mono_bold", 40)
        shown = int(round(p * len(tokens)))
        x, y = x0 + 20, y0 + (y1 - y0) * 0.62
        d.rounded_rectangle([x0, y - 20, x1, y1 - (60 if note else 0)], radius=14, fill=PANEL)
        for text, kind in tokens[:shown]:
            width = font.getlength(text)
            if x + width > x1 - 20:
                x, y = x0 + 20, y + 56
            d.text((x, y), text, font=font, fill=DIM if kind == "lit" else YELLOW)
            x += width
        if note:
            caption(img, box, note)
    return draw


# ---------------------------------------------------------------- бенчмарки

OPS = [("insert", "вставка", BLUE), ("find", "поиск", GREEN), ("remove", "удаление", RED),
       ("traverse", "обход", ORANGE), ("serialize", "строка", PURPLE)]


def load_bench(path):
    data = {}
    if not os.path.exists(path):
        return data
    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            size = int(row["size"])
            entry = data.setdefault(row["operation"], {})
            tree_ms = float(row["tree_ms"]) if row["tree_ms"] else None
            set_ms = float(row["set_ms"]) if row["set_ms"] else None
            entry[size] = (tree_ms, set_ms, float(row["tree_memory_mb"] or "nan"))
    return data


def bench_fig(data, ops, mode="per_element", show_set=False, title=""):
    """mode: per_element - нс на элемент; total - мс; memory - МБ."""
    def value(size, tree_ms, set_ms, mem, of_set):
        if mode == "memory":
            return mem
        ms = set_ms if of_set else tree_ms
        if ms is None:
            return None
        return ms * 1e6 / size if mode == "per_element" else ms

    def draw(img, box, p):
        d = ImageDraw.Draw(img)
        x0, y0, x1, y1 = box
        px0, py0, px1, py1 = x0 + 120, y0 + 60, x1 - 30, y1 - 90
        d.rectangle([px0, py0, px1, py1], outline=(70, 76, 100), width=2)
        d.text((x0, y0), title, font=Fonts.get("bold", 30), fill=TEXT)
        series = []
        for op, name, color in ops:
            if op not in data:
                continue
            for of_set in ([False, True] if show_set else [False]):
                points = []
                for size in sorted(data[op]):
                    tree_ms, set_ms, mem = data[op][size]
                    v = value(size, tree_ms, set_ms, mem, of_set)
                    if v is not None and not math.isnan(v) and v > 0:
                        points.append((size, v))
                if points:
                    series.append((name + (" (std::set)" if of_set else ""), color, of_set,
                                   points))
        if not series:
            text_center(d, ((x0 + x1) / 2, (y0 + y1) / 2), "нет данных", Fonts.get("bold", 40),
                        DIM)
            return
        xs = [s for _, _, _, pts in series for s, _ in pts]
        ys = [v for _, _, _, pts in series for _, v in pts]
        lx0, lx1 = math.log10(min(xs)), math.log10(max(xs))
        log_y = mode != "per_element"
        if log_y:
            ly0, ly1 = math.floor(math.log10(min(ys))), math.ceil(math.log10(max(ys)))
        else:
            ly0, ly1 = 0, max(ys) * 1.1

        def to_px(xv, yv):
            fx = (math.log10(xv) - lx0) / max(lx1 - lx0, 1e-9)
            yy = math.log10(yv) if log_y else yv
            fy = (yy - ly0) / max(ly1 - ly0, 1e-9)
            return px0 + fx * (px1 - px0), py1 - fy * (py1 - py0)

        small = Fonts.get("regular", 22)
        for e in range(int(lx0), int(lx1) + 1):
            x, _ = to_px(10 ** e, 10 ** ly0 if log_y else 1)
            d.line([x, py0, x, py1], fill=(40, 44, 62), width=1)
            text_center(d, (x, py1 + 22), f"10^{e}", small, DIM)
        if log_y:
            for e in range(int(ly0), int(ly1) + 1):
                _, y = to_px(10 ** lx0, 10 ** e)
                d.line([px0, y, px1, y], fill=(40, 44, 62), width=1)
                label = f"10^{e}"
                d.text((px0 - 80, y - 12), label, font=small, fill=DIM)
        else:
            raw_step = ly1 / 5
            magnitude = 10 ** math.floor(math.log10(raw_step))
            tick = next(m * magnitude for m in (1, 2, 2.5, 5, 10) if m * magnitude >= raw_step)
            yv = 0.0
            while yv <= ly1:
                _, y = to_px(10 ** lx0, yv)
                d.line([px0, y, px1, y], fill=(40, 44, 62), width=1)
                d.text((px0 - 90, y - 12), f"{yv:.0f}", font=small, fill=DIM)
                yv += tick
        text_center(d, ((px0 + px1) / 2, py1 + 58), "n (элементов), лог. шкала", small, DIM)

        reveal = smooth(p)
        legend_y = py0 + 12
        for name, color, of_set, pts in series:
            pixels = [to_px(s, v) for s, v in pts]
            count = max(1, int(round(reveal * len(pixels))))
            visible = pixels[:count]
            if of_set:
                dashed_line(d, visible, color, 3)
            elif len(visible) > 1:
                d.line(visible, fill=color, width=5, joint="curve")
            for x, y in visible:
                r = 5 if of_set else 7
                d.ellipse([x - r, y - r, x + r, y + r], fill=color)
            d.line([px0 + 20, legend_y + 14, px0 + 60, legend_y + 14], fill=color,
                   width=3 if of_set else 5)
            d.text((px0 + 70, legend_y), name, font=small, fill=TEXT)
            legend_y += 32
    return draw
