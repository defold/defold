"""Reference quad mesh for *potential* tiled GUI Slice-9 rendering.

This is a deliberately isolated geometry oracle, not an engine implementation
or an agreed shader/material contract. It represents Option C from
proofs/defold-bounty/feature-design-tiled-slice9.md so that the expected atlas
sampling of Option A can be regression-tested later.

Coordinates:
  * node/source positions have origin at bottom-left, with +x right, +y up;
  * slice9 margins are (left, top, right, bottom), in source pixels;
  * atlas rectangle is (u_min, v_min, u_max, v_max);
  * rotated=True assumes the source is packed 90 degrees clockwise, matching
    the experimental contract in scripts/test_tiled_slice9_uv_contract.py.

No texture filtering or pixel-padding guarantees are made: an actual engine
shader still needs seam-bleeding and mipmap tests.

Run: python3 scripts/slice9_tiling_reference.py
Test: python3 -m unittest discover -s scripts -p 'test_slice9_tiling_reference.py' -v
"""

from dataclasses import dataclass
from math import ceil, isfinite
from typing import NamedTuple


class UV(NamedTuple):
    u: float
    v: float


@dataclass(frozen=True)
class Quad:
    """Axis-aligned output quad in destination pixels and clockwise UV corners.

    uv_bl, uv_tl, uv_tr, uv_br correspond to destination BL/TL/TR/BR even
    after a horizontal or vertical source-image flip.
    cell is (column, row) from left/bottom, with (1, 1) being the center.
    """

    cell: tuple[int, int]
    x0: float
    y0: float
    x1: float
    y1: float
    uv_bl: UV
    uv_tl: UV
    uv_tr: UV
    uv_br: UV

    @property
    def area(self) -> float:
        return (self.x1 - self.x0) * (self.y1 - self.y0)


def _segments(dst0: float, dst1: float, src0: float, src1: float, repeat: bool):
    """Yield clipped destination and sampled-source intervals, never stretch."""
    dst_span = dst1 - dst0
    src_span = src1 - src0
    if dst_span <= 0:
        return
    if src_span <= 0:
        raise ValueError("Nonempty destination Slice-9 cell has empty source")
    if not repeat:
        yield dst0, dst1, src0, src1
        return
    # Calculate the boundaries from the tile number, rather than repeatedly
    # adding source spans (which accumulates floating-point rounding error).
    for i in range(ceil(dst_span / src_span)):
        start = dst0 + i * src_span
        end = min(dst1, dst0 + (i + 1) * src_span)
        if end > start:
            yield start, end, src0, src0 + (end - start)


def tiled_slice9_quads(
    node_size: tuple[float, float],
    source_size: tuple[float, float],
    margins: tuple[float, float, float, float],
    atlas_rect: tuple[float, float, float, float],
    *,
    rotated: bool = False,
    flip_x: bool = False,
    flip_y: bool = False,
    max_quads: int = 4096,
) -> list[Quad]:
    """Return a clipped atlas-safe quad mesh for one nine-sliced GUI frame.

    Corners sample once; edge cells repeat in their long axis only; the center
    repeats in both directions. Partial final cells have proportionally
    clipped *source UVs*, never a stretched tile. No sampler REPEAT is needed.

    The prototype intentionally rejects nodes smaller than their summed
    margins. The engine has its own sub-minimum-size fallback policy; do not
    silently redefine that policy as part of a future tiled mode.
    """
    w, h = map(float, node_size)
    sw, sh = map(float, source_size)
    left, top, right, bottom = map(float, margins)
    u0, v0, u1, v1 = map(float, atlas_rect)
    values = (w, h, sw, sh, left, top, right, bottom, u0, v0, u1, v1)
    if not all(isfinite(x) for x in values):
        raise ValueError("All node/source/margin/UV coordinates must be finite")
    if min(w, h, sw, sh) <= 0:
        raise ValueError("Node and source frame dimensions must be positive")
    if min(left, top, right, bottom) < 0:
        raise ValueError("Slice-9 margins must be nonnegative")
    if left + right >= sw or top + bottom >= sh:
        raise ValueError("Source frame must contain a nonempty center cell")
    if left + right > w or top + bottom > h:
        raise ValueError("Node smaller than its Slice-9 margins is unsupported")
    if not (0 <= u0 < u1 <= 1 and 0 <= v0 < v1 <= 1):
        raise ValueError("Atlas rectangle must be inside the unit square")
    if isinstance(max_quads, bool) or not isinstance(max_quads, int) or max_quads <= 0:
        raise ValueError("max_quads must be a positive integer")

    dx = (0.0, left, w - right, w)
    dy = (0.0, bottom, h - top, h)
    sx = (0.0, left, sw - right, sw)
    sy = (0.0, bottom, sh - top, sh)
    quads: list[Quad] = []

    def to_uv(x: float, y: float) -> UV:
        # Explicit local atlas transform, not sampler wrap over the entire
        # atlas. Source coordinates are never allowed to cross another frame.
        tx, ty = x / sw, y / sh
        if rotated:
            tx, ty = ty, 1.0 - tx
        return UV(u0 + tx * (u1 - u0), v0 + ty * (v1 - v0))

    for col in range(3):
        for row in range(3):
            repeat_x = col == 1
            repeat_y = row == 1
            xsegs = list(_segments(dx[col], dx[col + 1], sx[col], sx[col + 1], repeat_x))
            ysegs = list(_segments(dy[row], dy[row + 1], sy[row], sy[row + 1], repeat_y))

            if len(quads) + len(xsegs) * len(ysegs) > max_quads:
                raise ValueError("Tile geometry exceeds max_quads; prefer a shader path")
            for ax, bx, au, bu in xsegs:
                for ay, by, av, bv in ysegs:
                    # Transform physical geometry, not atlas UVs. A flip
                    # swaps which original source margin appears on each side.
                    px0, px1 = (w - bx, w - ax) if flip_x else (ax, bx)
                    py0, py1 = (h - by, h - ay) if flip_y else (ay, by)
                    ux0, ux1 = (bu, au) if flip_x else (au, bu)
                    uy0, uy1 = (bv, av) if flip_y else (av, bv)
                    quads.append(
                        Quad(
                            (col, row), px0, py0, px1, py1,
                            to_uv(ux0, uy0), to_uv(ux0, uy1),
                            to_uv(ux1, uy1), to_uv(ux1, uy0),
                        )
                    )
    return quads


if __name__ == "__main__":
    demo = tiled_slice9_quads(
        (29, 22), (18, 14), (3, 2, 5, 4),
        (0.1, 0.2, 0.3, 0.4), rotated=True
    )
    print(f"Reference 9-slice: {len(demo)} quads / {len(demo) * 6} triangle vertices")
    print(f"Covered node pixels: {sum(q.area for q in demo):g} / {29 * 22}")
    print(f"Center-quads: {sum(q.cell == (1, 1) for q in demo)}")
