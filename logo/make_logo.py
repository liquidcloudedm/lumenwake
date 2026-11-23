# Draws the Lumenwake logo: a point of light and the wake it leaves behind.
# Usage: python make_logo.py [output.png] [size]
import sys

import numpy as np
from PIL import Image

out = sys.argv[1] if len(sys.argv) > 1 else "lumenwake-logo-1024.png"
size = int(sys.argv[2]) if len(sys.argv) > 2 else 1024

SS = 2                          # drawn at twice the size, then scaled down
N = size * SS
u = N / 1024.0                  # all measurements below are in 1024-pixel units

y, x = np.mgrid[0:N, 0:N].astype(np.float64)
x = (x + 0.5) / u
y = (y + 0.5) / u


def rgb(h):
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64) / 255.0


BACKGROUND = rgb("0e0d10")
LIFT = rgb("1b1820")
AMBER = rgb("ffb454")
CORE = rgb("fff1d6")

source = np.array([768.0, 256.0])       # the light
direction = np.radians(135.0)           # the wake runs down and to the left
half_width = np.radians(50.0)
dot_radius = 60.0
thickness = 46.0

# radius, colour
arcs = [(175.0, rgb("ffb454")),
        (315.0, rgb("ff8f5a")),
        (455.0, rgb("e85d86")),
        (595.0, rgb("9a55d8"))]

dx, dy = x - source[0], y - source[1]
r = np.hypot(dx, dy)
theta = np.arctan2(dy, dx) - direction
theta = (theta + np.pi) % (2.0 * np.pi) - np.pi


def coverage(distance, radius):
    """1 inside a shape of the given radius, 0 outside, one pixel of blend at the edge."""
    return np.clip((radius - distance) * u + 0.5, 0.0, 1.0)


# Background: almost black, a little lighter behind the mark.
centre = np.hypot(x - 512.0, y - 512.0) / 724.0
image = BACKGROUND + (LIFT - BACKGROUND) * np.clip(1.0 - centre, 0.0, 1.0)[..., None] ** 2

# A soft glow around the light.
glow = np.exp(-(r / 210.0) ** 2) * 0.20 + np.exp(-(r / 95.0) ** 2) * 0.30
image = image + AMBER * glow[..., None]

# The wake: arcs with round ends, each a step further from amber.
for radius, colour in arcs:
    to_end = np.full(r.shape, np.inf)

    for end in (-half_width, half_width):
        ex = source[0] + radius * np.cos(direction + end)
        ey = source[1] + radius * np.sin(direction + end)
        to_end = np.minimum(to_end, np.hypot(x - ex, y - ey))

    distance = np.where(np.abs(theta) <= half_width, np.abs(r - radius), to_end)
    a = coverage(distance, thickness * 0.5)[..., None]
    image = image * (1.0 - a) + colour * a

# The light itself.
a = coverage(r, dot_radius)[..., None]
shade = np.clip(1.0 - r / dot_radius, 0.0, 1.0)[..., None]
image = image * (1.0 - a) + (AMBER + (CORE - AMBER) * shade ** 0.8) * a

pixels = (np.clip(image, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
Image.fromarray(pixels, "RGB").resize((size, size), Image.LANCZOS).save(out)
print("wrote", out, size)
