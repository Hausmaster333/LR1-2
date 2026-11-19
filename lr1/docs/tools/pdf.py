# DESIGN.md -> DESIGN.pdf. Картинка ![..](docs/x.png) в PDF заменяется на docs/x.svg (векторная, чёткая)
# и раскладывается по размеру: небольшая - в тексте, крупная - на отдельной странице, при
# необходимости альбомной. pandoc собирает HTML, headless Edge печатает его в PDF
import pathlib
import re
import subprocess
import sys
import tempfile

from svgkit import EDGE

CSS = pathlib.Path(__file__).with_name("design.css")
PORTRAIT = (680, 1000)    # область текста A4 при полях 16/15 мм, CSS px
INLINE_HEIGHT = 880
INLINE_MIN_SCALE = 0.6    # если в ширину текста картинка влезает только мельче - отдельная страница
LANDSCAPE = (1030, 690)   # A4 альбомная, поля 12 мм
IMAGE = re.compile(r"^!\[([^\]]*)\]\(([^)]+)\)$")


def svg_size(path):
    head = path.read_text(encoding="utf-8")[:600]
    return (float(re.search(r'width="([\d.]+)"', head).group(1)),
            float(re.search(r'height="([\d.]+)"', head).group(1)))


def to_html(markdown):
    return subprocess.run(["pandoc", "-f", "gfm", "-t", "html5"], input=markdown, capture_output=True,
                          text=True, encoding="utf-8", check=True).stdout


def figure(alt, src, root, caption):
    """Картинка как HTML; крупная - на отдельной странице вместе с подписью. Возвращает (html, подпись забрана)."""
    svg = (root / src).with_suffix(".svg")
    if not svg.exists():
        return f"![{alt}]({src})", False
    w, h = svg_size(svg)
    rel = svg.relative_to(root).as_posix()
    inline = min(PORTRAIT[0] / w, INLINE_HEIGHT / h, 1.0)
    if inline >= INLINE_MIN_SCALE:
        return f'<p class="fig"><img src="{rel}" alt="{alt}" style="width:{w * inline:.0f}px"></p>', False
    reserve = 70 if caption else 0
    portrait = min(PORTRAIT[0] / w, (PORTRAIT[1] - reserve) / h, 1.0)
    landscape = min(LANDSCAPE[0] / w, (LANDSCAPE[1] - reserve) / h, 1.0)
    scale, cls = (landscape, "fig-page landscape") if landscape > portrait else (portrait, "fig-page")
    head = to_html(caption) if caption else ""
    return f'<div class="{cls}">\n{head}<img src="{rel}" alt="{alt}" style="width:{w * scale:.0f}px">\n</div>', bool(caption)


def layout(text, root):
    out = []
    for block in text.split("\n\n"):
        match = IMAGE.match(block.strip())
        if not match:
            out.append(block)
            continue
        previous = out[-1].strip() if out else ""
        # Подпись - заголовок или короткая строка прямо перед картинкой: уходит на ту же страницу
        is_caption = (previous and "\n" not in previous and len(previous) < 160
                      and not previous.startswith(("|", "-", "<", "!")))
        html, taken = figure(match.group(1), match.group(2), root, previous if is_caption else "")
        if taken:
            out.pop()
        out.append(html)
    return "\n\n".join(out)


def build(md):
    md = pathlib.Path(md).resolve()
    root = md.parent
    text = layout(md.read_text(encoding="utf-8"), root)
    tmp = pathlib.Path(tempfile.mkdtemp())
    source, html = tmp / "design.md", tmp / "design.html"
    source.write_text(text, encoding="utf-8")
    title = text.splitlines()[0].lstrip("# ").strip()
    subprocess.run(["pandoc", str(source), "-f", "gfm", "-t", "html5", "--standalone", "--embed-resources",
                    "--css", str(CSS), "--metadata", f"title={title}", "--variable", "title=",
                    "--resource-path", str(root), "-o", str(html)], check=True, cwd=root)
    pdf = md.with_suffix(".pdf")
    subprocess.run([EDGE, "--headless", "--disable-gpu", "--no-pdf-header-footer", f"--print-to-pdf={pdf}",
                    html.as_uri()], capture_output=True, timeout=180)
    print(pdf.name, pdf.stat().st_size, "bytes")


if __name__ == "__main__":
    for argument in sys.argv[1:]:
        build(argument)
