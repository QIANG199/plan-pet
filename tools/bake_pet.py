"""Bake clawd GIFs onto dark/light cards as RGB565 frames for the ESP32."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageSequence

ROOT = Path(__file__).resolve().parents[1]
GIF_DIR = Path(r"D:\develop\clawd-on-desk\assets\gif")
OUT_BIN = ROOT / "firmware" / "src" / "pet_frames.bin"
OUT_S = ROOT / "firmware" / "src" / "pet_blob.S"

CANVAS = 160
SPRITE = 150
FRAME_COUNT = 6
STATES = (
    "idle",
    "thinking",
    "typing",
    "happy",
    "error",
    "sleeping",
    "poke",
)
STATE_GIF = {
    "idle": "clawd-idle.gif",
    "thinking": "clawd-thinking.gif",
    "typing": "clawd-typing.gif",
    "happy": "clawd-happy.gif",
    "error": "clawd-error.gif",
    "sleeping": "clawd-sleeping.gif",
    "poke": "clawd-react-double-jump.gif",
}
THEMES = (
    ("dark", (0x1A, 0x1E, 0x26)),
    ("light", (0xFF, 0xFF, 0xFF)),
)


def gif_frames(path: Path) -> list[Image.Image]:
    im = Image.open(path)
    canvas = Image.new("RGBA", im.size, (0, 0, 0, 0))
    prev = canvas.copy()
    out: list[Image.Image] = []
    for frame in ImageSequence.Iterator(im):
        cur = prev.copy()
        fr = frame.convert("RGBA")
        cur.paste(fr, (0, 0), fr)
        out.append(cur.copy())
        disposal = getattr(frame, "disposal_method", 1)
        if disposal == 2:
            prev = canvas.copy()
        else:
            prev = cur
    return out


def pick(frames: list[Image.Image], n: int) -> list[Image.Image]:
    if len(frames) <= n:
        return frames
    last = len(frames) - 1
    return [frames[round(i * last / (n - 1))] for i in range(n)]


def union_bbox(frames: list[Image.Image]) -> tuple[int, int, int, int]:
    box: list[int] | None = None
    for fr in frames:
        b = fr.getbbox()
        if not b:
            continue
        if box is None:
            box = list(b)
        else:
            box[0] = min(box[0], b[0])
            box[1] = min(box[1], b[1])
            box[2] = max(box[2], b[2])
            box[3] = max(box[3], b[3])
    if box is None:
        raise SystemExit("gif has no opaque pixels")
    pad = 8
    return (
        max(0, box[0] - pad),
        max(0, box[1] - pad),
        box[2] + pad,
        box[3] + pad,
    )


def prepare_light_sprite(sprite: Image.Image) -> Image.Image:
    """Make white fills visible on a white card and add a 1px ink outline."""
    w, h = sprite.size
    src = sprite.load()
    out = sprite.copy()
    dst = out.load()
    fill = (0xC5, 0xCD, 0xD8, 255)
    outline = (0x5A, 0x65, 0x75, 255)
    for y in range(h):
        for x in range(w):
            r, g, b, a = src[x, y]
            if a < 128:
                continue
            if min(r, g, b) >= 210 and max(r, g, b) - min(r, g, b) <= 40:
                dst[x, y] = fill
    edged = out.copy()
    ep = edged.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = ep[x, y]
            if a < 128:
                continue
            edge = False
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if nx < 0 or ny < 0 or nx >= w or ny >= h or ep[nx, ny][3] < 128:
                    edge = True
                    break
            if edge:
                dst[x, y] = outline
    return out


def fit_on_card(
    src: Image.Image,
    bg: tuple[int, int, int],
    crop: tuple[int, int, int, int],
    light: bool,
) -> Image.Image:
    card = Image.new("RGBA", (CANVAS, CANVAS), bg + (255,))
    sprite = src.crop(crop)
    ratio = min(SPRITE / sprite.width, SPRITE / sprite.height)
    w = max(1, round(sprite.width * ratio))
    h = max(1, round(sprite.height * ratio))
    sprite = sprite.resize((w, h), Image.Resampling.LANCZOS)
    if light:
        sprite = prepare_light_sprite(sprite)
    x = (CANVAS - w) // 2
    y = (CANVAS - h) // 2
    card.paste(sprite, (x, y), sprite)
    return card.convert("RGB")


def rgb565(rgb: Image.Image) -> bytes:
    px = rgb.tobytes()
    out = bytearray(len(px) // 3 * 2)
    j = 0
    for i in range(0, len(px), 3):
        r, g, b = px[i], px[i + 1], px[i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[j] = v & 0xFF
        out[j + 1] = v >> 8
        j += 2
    return bytes(out)


def main() -> None:
    blob = bytearray()
    for theme_name, bg in THEMES:
        for state in STATES:
            path = GIF_DIR / STATE_GIF[state]
            if not path.exists():
                raise SystemExit(f"missing {path}")
            all_frames = gif_frames(path)
            crop = union_bbox(all_frames)
            frames = pick(all_frames, FRAME_COUNT)
            print(
                f"{theme_name:5} {state:9} src={len(all_frames)} crop={crop}"
            )
            for fr in frames:
                blob.extend(rgb565(fit_on_card(fr, bg, crop, theme_name == "light")))
    expected = 2 * len(STATES) * FRAME_COUNT * CANVAS * CANVAS * 2
    if len(blob) != expected:
        raise SystemExit(f"size {len(blob)} != {expected}")
    OUT_BIN.parent.mkdir(parents=True, exist_ok=True)
    OUT_BIN.write_bytes(blob)
    bin_posix = OUT_BIN.resolve().as_posix()
    OUT_S.write_text(
        f"# pet_frames {len(blob)} bytes\n"
        f"    .section .rodata\n"
        f"    .align 4\n"
        f"    .global pet_frames_bin\n"
        f"pet_frames_bin:\n"
        f"    .incbin \"{bin_posix}\"\n",
        encoding="utf-8",
    )
    print(f"wrote {OUT_BIN} ({len(blob)} bytes, {len(blob)/1048576:.2f} MiB)")


if __name__ == "__main__":
    main()
