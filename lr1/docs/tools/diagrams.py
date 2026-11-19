import json

from svgkit import Diagram


def render(source, target):
    spec = json.loads(source.read_text(encoding="utf-8"))
    diagram = Diagram(spec["width"], spec["height"])
    for panel in spec["panels"]:
        diagram.panel(**panel)
    for box in spec["boxes"]:
        diagram.box(**box)
    for edge in spec["edges"]:
        options = edge.copy()
        start = options.pop("from")
        end = options.pop("to")
        diagram.edge(start, end, **options)
    for arrow in spec.get("arrows", []):
        diagram.arrow(**arrow)
    for label in spec.get("labels", []):
        diagram.label(label["x"], label["y"], label["text"], label.get("anchor", "middle"))
    diagram.save(target)


def draw(docs):
    for name in ("entities", "architecture"):
        render(docs / f"{name}.diagram.json", docs / f"{name}.svg")
