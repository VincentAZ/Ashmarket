#!/usr/bin/env python3
"""Turns a photograph of red liquid on a white background into a decal texture: the same picture with the white
taken out. Thin liquid over white photographs as pale red, so how see-through a pixel is is read from how much
green is left in it, and the white is un-mixed from its colour. The glints of the studio lights are filled with
the liquid's own colour, because in the game the lights of the cage put their own glints on it.

    $BLENDER -b --factory-startup -P Tools/key_blood_decal.py -- <picture on white> <decal .png>

It runs inside a Blender whose Python has numpy, for the picture reading and writing; nothing of Blender's is used to draw.

Nothing is drawn: every pixel of the result comes from the picture.
"""
import sys
from collections import deque

import bpy
import numpy as np


def main(source, target):
    loaded = bpy.data.images.load(source)
    width, height = loaded.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    loaded.pixels.foreach_get(pixels)
    # as the eye sees it, not as light: the picture's own sRGB values
    loaded.colorspace_settings.name = "sRGB"
    picture = pixels.reshape(height, width, 4)[..., :3]
    picture = np.where(picture <= 0.0031308, picture * 12.92, 1.055 * np.power(np.maximum(picture, 0.0), 1 / 2.4) - 0.055) if loaded.is_float else picture
    red, green, blue = picture[..., 0], picture[..., 1], picture[..., 2]

    # opacity: white has all its green, thick liquid has none
    # the white of a photograph is never quite white: what is nearly white is all white
    opacity = np.clip((1.0 - np.minimum(green, blue) - 0.12) / 0.8, 0.0, 1.0)

    # what is inside the liquid: everything the white background cannot reach from the edge of the picture
    outside = np.zeros(opacity.shape, dtype=bool)
    open_pixels = opacity < 0.25
    queue = deque()
    height, width = opacity.shape
    for x in range(width):
        queue.append((0, x)); queue.append((height - 1, x))
    for y in range(height):
        queue.append((y, 0)); queue.append((y, width - 1))
    while queue:
        y, x = queue.popleft()
        if 0 <= y < height and 0 <= x < width and open_pixels[y, x] and not outside[y, x]:
            outside[y, x] = True
            queue.extend(((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)))
    glints = open_pixels & ~outside

    # un-mix the white from the colour, then give the glints the colour of the liquid round them
    safe = np.maximum(opacity, 0.05)[..., None]
    colour = np.clip((picture - (1.0 - opacity[..., None])) / safe, 0.0, 1.0)
    body = (opacity > 0.8) & ~glints
    colour[glints | (opacity < 0.05)] = np.median(colour[body], axis=0) if body.any() else (0.3, 0.0, 0.0)
    opacity[glints] = 1.0

    result = np.dstack([colour, opacity[..., None]]).astype(np.float32)
    decal = bpy.data.images.new("decal", width, height, alpha=True)
    decal.alpha_mode = "STRAIGHT"
    decal.pixels.foreach_set(result.ravel())
    decal.filepath_raw = target
    decal.file_format = "PNG"
    decal.save()
    print("%s: %d%% covered, %d glint pixels filled" % (target, round(100.0 * float((opacity > 0.5).mean())), int(glints.sum())))


main(*sys.argv[sys.argv.index("--") + 1:][:2])
