"""
Second step of wearing the Gate Warden in (Tools/wear_gate_warden.sh): from the pictures his armor and his face are drawn
from (ArtSource/worn/gate_warden/src, written by the first step) makes worn ones in ArtSource/worn/gate_warden/out.

Nothing is drawn here. What is laid on is photographed: ambientCG's scans of dust and smudges on a surface, of scratches,
and of smears (ArtSource/ambientcg, CC0). Where it is laid is read from the pictures themselves: dust gathers where the
armor's own occlusion map says a crevice is, scratches show on the hard parts along the edges its own normal map has,
and the cloth is left its weave. On the face, dust sits in the creases the skin's own picture has, the roughness of the
forehead and nose is brought down in patches as sweat does it, and the colour of nose, cheeks and ears and under the
eyes is shifted a little, as weather and no sleep shift it (the one thing here that is a judgement and not a photograph).

    <python with numpy, scipy and PIL> Tools/wear_gate_warden_paint.py [--much 1.0]

--much scales all of it: 0 is as bought, 1 is as meant, 1.5 is filthy.
"""

import json
import pathlib
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

Image.MAX_IMAGE_PIXELS = None

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "ArtSource/worn/gate_warden/src"
OUT = ROOT / "ArtSource/worn/gate_warden/out"
SCANS = ROOT / "ArtSource/ambientcg"

MUCH = float(sys.argv[sys.argv.index("--much") + 1]) if "--much" in sys.argv else 1.0

# the dust of the place: pale desert sand, as it lies on something dark
SAND = np.array([0.56, 0.48, 0.38], dtype=np.float32)


def load(path):
    return np.asarray(Image.open(path)).astype(np.float32) / 255.0


def scan(name, which="opacity"):
    grey = np.asarray(Image.open(SCANS / name / "{}_{}.jpg".format(name, which)).convert("L")).astype(np.float32) / 255.0

    # from its dark to its bright, whatever the scan's own exposure
    low, high = np.percentile(grey, [3, 99.5])

    return np.clip((grey - low) / max(high - low, 1e-3), 0.0, 1.0)


def laid(grey, size, times, seed):
    """A scan laid over a picture of a given size that many times across, begun somewhere of its own."""
    rng = np.random.default_rng(seed)
    side = max(size // times, 8)
    small = np.asarray(Image.fromarray((grey * 255).astype(np.uint8)).resize((side, side), Image.LANCZOS)).astype(np.float32) / 255.0

    if rng.random() < 0.5:
        small = small[:, ::-1]

    small = np.rot90(small, int(rng.integers(0, 4)))
    whole = np.tile(small, (times + 1, times + 1))
    whole = np.roll(whole, (int(rng.integers(0, side)), int(rng.integers(0, side))), axis=(0, 1))

    return whole[:size, :size]


def soft(grey, radius):
    return ndimage.gaussian_filter(grey, radius)


def save(array, name):
    OUT.mkdir(parents=True, exist_ok=True)
    Image.fromarray((np.clip(array, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)).save(OUT / name, compress_level=3)


def wear_armor(colour_file, packed_file, normal_file, seed):
    colour = load(SRC / colour_file)
    packed = load(SRC / packed_file)
    size = colour.shape[0]

    if packed.shape[0] != size:
        packed = np.asarray(Image.fromarray((packed * 255).astype(np.uint8)).resize((size, size), Image.BILINEAR)).astype(np.float32) / 255.0

    occlusion, rough, metal = packed[..., 0], packed[..., 1], packed[..., 2]

    # where a crevice is, and the wider hollow it is in
    crevice = np.clip((1.0 - occlusion) * 2.2, 0.0, 1.0)
    hollow = soft(crevice, size / 170.0)
    crevice = soft(crevice, size / 1400.0)

    dust = np.clip(0.65 * laid(scan("SurfaceImperfections004"), size, 3, seed) + 0.55 * laid(scan("SurfaceImperfections013"), size, 2, seed + 1), 0.0, 1.0)
    smudge = laid(scan("SurfaceImperfections008"), size, 2, seed + 2)

    lies = np.clip((0.07 + 0.5 * crevice + 0.5 * dust * (0.3 + 0.7 * hollow) + 0.12 * smudge) * MUCH, 0.0, 0.82)

    light = colour[..., :3].mean(axis=2)
    light = light / max(float(np.percentile(light, 99)), 1e-3)

    worn = colour.copy()
    worn[..., :3] = colour[..., :3] * (1.0 - lies[..., None]) + (SAND * (0.42 + 0.58 * np.clip(light, 0.0, 1.0))[..., None]) * lies[..., None]

    # Scratches, on what is hard (plate, buckle, plastic: smooth or metal in the armor's own picture), most along its
    # edges, which the normal map has as where the surface turns quickly
    hard = np.clip(np.maximum((0.5 - rough) * 4.0, metal * 3.0), 0.0, 1.0)
    hard = soft(hard, size / 2000.0)

    scratched = np.clip(0.75 * laid(scan("Scratches004"), size, 3, seed + 3) + 0.6 * laid(scan("Scratches003"), size, 2, seed + 4), 0.0, 1.0)

    edge = np.zeros_like(rough)

    if normal_file and (SRC / normal_file).exists():
        normal = load(SRC / normal_file)

        if normal.shape[0] != size:
            normal = np.asarray(Image.fromarray((normal * 255).astype(np.uint8)).resize((size, size), Image.BILINEAR)).astype(np.float32) / 255.0

        turn = np.hypot(ndimage.sobel(normal[..., 0], axis=0), ndimage.sobel(normal[..., 0], axis=1)) + np.hypot(ndimage.sobel(normal[..., 1], axis=0), ndimage.sobel(normal[..., 1], axis=1))
        edge = np.clip(soft(turn, size / 900.0) * 2.2, 0.0, 1.0)

    bare = np.clip(scratched * (0.22 + 1.6 * edge) * hard * MUCH, 0.0, 0.85)

    # what is under the finish: dull pale grey
    worn[..., :3] = worn[..., :3] * (1.0 - bare[..., None]) + (0.3 + 0.35 * worn[..., :3]) * bare[..., None]

    made = packed.copy()

    smear = laid(scan("Smear003", "roughness"), size, 2, seed + 5)

    # nothing that has been worn in a desert is glossy: dust dulls it, a scratch dulls it, a hand's smear breaks it up
    made[..., 1] = np.clip(rough + (1.0 - rough) * (0.22 * MUCH + 0.55 * lies) + 0.2 * bare + (smear - 0.5) * 0.16 * MUCH, 0.0, 1.0)
    made[..., 2] = np.clip(metal * (1.0 - 0.75 * lies), 0.0, 1.0)
    made[..., 0] = np.clip(occlusion * (1.0 - 0.18 * lies), 0.0, 1.0)

    save(worn, colour_file.replace(".png", "_Worn.png"))
    save(made, packed_file.replace(".png", "_Worn.png"))

    print("{}: dust on {:.0%} of it on average, bare finish on {:.1%}; roughness {:.2f} -> {:.2f}".format(colour_file, float(lies.mean()), float((bare > 0.25).mean()), float(rough.mean()), float(made[..., 1].mean())))


def spot(size, x, y, wide, high):
    """A soft patch about a place on the face's picture, all in shares of the picture's side."""
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32) / size

    return np.exp(-(((xs - x) / wide) ** 2 + ((ys - y) / high) ** 2))


def wear_face():
    colour = load(SRC / "T_Head_LOD1_BC_VT.png")
    packed = load(SRC / "T_Head_LOD1_SRMF_VT.png")
    size = colour.shape[0]

    light = colour[..., :3].mean(axis=2)

    # the creases and pores the skin's own picture has: darker than what is round them
    crease = np.clip((soft(light, size / 340.0) - light) * 14.0, 0.0, 1.0)
    crease = soft(crease, size / 1600.0)

    dust = laid(scan("SurfaceImperfections001"), size, 2, 11)
    smudge = laid(scan("SurfaceImperfections008"), size, 1, 12)

    # Where on the face. The picture is laid out as every MetaHuman head's is: the face in the middle, forehead above,
    # ears at either side, the neck below.
    forehead = spot(size, 0.5, 0.22, 0.2, 0.09)
    nose = spot(size, 0.5, 0.47, 0.045, 0.075)
    cheeks = spot(size, 0.335, 0.5, 0.075, 0.07) + spot(size, 0.665, 0.5, 0.075, 0.07)
    ears = spot(size, 0.15, 0.45, 0.05, 0.09) + spot(size, 0.85, 0.45, 0.05, 0.09)
    under_eyes = spot(size, 0.39, 0.41, 0.055, 0.022) + spot(size, 0.61, 0.41, 0.055, 0.022)
    upper_lip = spot(size, 0.5, 0.55, 0.06, 0.02)
    temples = spot(size, 0.27, 0.3, 0.06, 0.08) + spot(size, 0.73, 0.3, 0.06, 0.08)
    neck = np.clip((np.mgrid[0:size, 0:size][0].astype(np.float32) / size - 0.74) * 6.0, 0.0, 1.0)
    eyes_and_mouth = spot(size, 0.395, 0.375, 0.07, 0.03) + spot(size, 0.605, 0.375, 0.07, 0.03) + spot(size, 0.5, 0.6, 0.09, 0.035)

    # dirt: in the creases everywhere, in the hairline and behind the ears and down the neck, never on the eyes or the lips
    lies = (0.035 + 0.3 * crease + 0.1 * dust * (0.4 + forehead + neck + ears) + 0.1 * smudge * neck) * MUCH
    lies = np.clip(lies * (1.0 - np.clip(eyes_and_mouth, 0.0, 1.0)), 0.0, 0.42)

    worn = colour.copy()
    worn[..., :3] = colour[..., :3] * (1.0 - lies[..., None]) + (colour[..., :3] * 0.5 + SAND * 0.26) * lies[..., None]

    # weather on what sticks out, and nights without sleep under the eyes
    red = np.clip(0.5 * nose + 0.5 * cheeks + 0.6 * ears, 0.0, 1.0) * MUCH
    worn[..., 0] *= 1.0 + 0.07 * red
    worn[..., 1] *= 1.0 - 0.07 * red
    worn[..., 2] *= 1.0 - 0.08 * red

    tired = np.clip(under_eyes, 0.0, 1.0) * MUCH
    worn[..., 0] *= 1.0 - 0.13 * tired
    worn[..., 1] *= 1.0 - 0.12 * tired
    worn[..., 2] *= 1.0 - 0.07 * tired

    made = packed.copy()

    # Sweat: the skin's roughness brought down in broken patches on forehead, nose, upper lip and temples. Dust the other
    # way wherever it lies. The second channel of this picture is the skin's roughness.
    patches = np.clip(laid(scan("Fingerprints003", "roughness"), size * 1, 2, 13)[:size, :size] if packed.shape[0] == size else 0.5, 0.0, 1.0)

    if packed.shape[0] != size:
        scale = packed.shape[0] // size

        def up(grey):
            return np.asarray(Image.fromarray((np.clip(grey, 0.0, 1.0) * 255).astype(np.uint8)).resize((packed.shape[1], packed.shape[0]), Image.BILINEAR)).astype(np.float32) / 255.0

        wet = up(np.clip(0.85 * forehead + 0.9 * nose + 0.8 * upper_lip + 0.6 * temples, 0.0, 1.0))
        dry = up(lies / 0.42)
        patches = 0.45 + 0.55 * laid(scan("SurfaceImperfections008"), packed.shape[0], 3, 13)
    else:
        wet = np.clip(0.85 * forehead + 0.9 * nose + 0.8 * upper_lip + 0.6 * temples, 0.0, 1.0)
        dry = lies / 0.42
        patches = 0.45 + 0.55 * laid(scan("SurfaceImperfections008"), size, 3, 13)

    made[..., 1] = np.clip(packed[..., 1] * (1.0 - 0.38 * wet * patches * MUCH) + 0.14 * dry * (1.0 - wet), 0.0, 1.0)

    # and the shine itself up a little where it is wet: the first channel is how strongly the skin mirrors
    made[..., 0] = np.clip(packed[..., 0] * (1.0 + 0.25 * wet * patches * MUCH), 0.0, 1.0)

    save(worn, "T_Head_LOD1_BC_VT_Worn.png")
    save(made, "T_Head_LOD1_SRMF_VT_Worn.png")

    print("face: dirt on {:.0%} of it on average; roughness on the forehead {:.2f} -> {:.2f}".format(
        float(lies.mean()), float((packed[..., 1] * wet).sum() / wet.sum()), float((made[..., 1] * wet).sum() / wet.sum())))


def main():
    listed = json.loads((SRC / "pictures.json").read_text())
    done = set()

    for index, material in enumerate(listed["materials"]):
        colour = packed = normal = None

        for facts in material["pictures"].values():
            if "base_color" in facts["roles"]:
                colour = facts["file"]
            elif "roughness" in facts["roles"]:
                packed = facts["file"]
            elif "normal" in facts["roles"]:
                normal = facts["file"]

        if colour and packed and colour not in done:
            done.add(colour)
            wear_armor(colour, packed, normal, 100 + index * 7)

    wear_face()


main()
