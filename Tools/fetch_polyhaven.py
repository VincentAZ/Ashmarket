#!/usr/bin/env python3
"""Downloads scanned and artist-made assets from Poly Haven (polyhaven.com, all CC0) into ArtSource/polyhaven/<id>/.

    Tools/fetch_polyhaven.py [--res 2k] <id> [<id> ...]             models: the glTF file with its geometry and textures
    Tools/fetch_polyhaven.py --surface [--res 4k] <id> [<id> ...]   surfaces: the colour, normal and roughness maps as PNG
    Tools/fetch_polyhaven.py --sky [--res 4k] <id> [<id> ...]       skies: the photographed sky as one .hdr file

Nothing is made here: the files are stored as Poly Haven publishes them.
"""
import argparse
import json
import os
import re
import subprocess
import urllib.request

API = "https://api.polyhaven.com/files/"
HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ArtSource", "polyhaven")
# Poly Haven's name for a map -> ours
MAPS = {"Diffuse": "basecolor", "nor_gl": "normal", "Rough": "roughness", "Metal": "metallic"}


def get(url, path=None):
    request = urllib.request.Request(url, headers={"User-Agent": "asset-fetch/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read()
    if path:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as out:
            out.write(data)
    return data


def restore_cutouts(asset, files, folder, res):
    """Poly Haven's glTF for a see-through part (fence wire, say) names it as see-through but ships its colour as a JPEG,
    which cannot hold the holes; the holes are published as a separate opacity map. This puts that map back where the
    glTF format keeps it, in the colour image's alpha channel, and marks the material as cut out. Nothing is drawn."""
    path = os.path.join(folder, asset + ".gltf")
    with open(path) as source:
        gltf = json.load(source)
    done = []
    for material in gltf.get("materials", []):
        colour = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
        if material.get("alphaMode") not in ("BLEND", "MASK") or colour is None:
            continue
        image = gltf["images"][gltf["textures"][colour["index"]]["source"]]
        part = re.match(r"textures/%s_(.*)_diff_\w+\.jpg$" % re.escape(asset), image["uri"])
        if not part or part.group(1) + "_opacity" not in files:
            continue
        maps = {}
        for kind in ("diff", "opacity"):
            sizes = files["%s_%s" % (part.group(1), kind)]
            maps[kind] = os.path.join(folder, "textures", "%s_%s_%s.png" % (asset, part.group(1), kind))
            get(sizes[res if res in sizes else sorted(sizes)[0]]["png"]["url"], maps[kind])
        merged = "textures/%s_%s_diff_cutout.png" % (asset, part.group(1))
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", maps["diff"], "-i", maps["opacity"],
                        "-filter_complex", "[1]format=gray[a];[0][a]alphamerge", os.path.join(folder, merged)], check=True)
        image["uri"] = merged
        image["mimeType"] = "image/png"
        material["alphaMode"] = "MASK"
        material["alphaCutoff"] = 0.5
        done.append(os.path.basename(merged))
    if done:
        with open(path, "w") as out:
            json.dump(gltf, out)
    return done


def fetch(asset, res, surface, sky):
    files = json.loads(get(API + asset))
    folder = os.path.join(HERE, asset)
    got = []
    if sky:
        sizes = files["hdri"]
        get(sizes[res if res in sizes else sorted(sizes)[0]]["hdr"]["url"], os.path.join(folder, asset + ".hdr"))
        got.append("sky")
    elif surface:
        for theirs, ours in MAPS.items():
            if theirs in files:
                sizes = files[theirs]
                get(sizes[res if res in sizes else sorted(sizes)[0]]["png"]["url"], os.path.join(folder, "%s_%s.png" % (asset, ours)))
                got.append(ours)
    else:
        package = files["gltf"][res if res in files["gltf"] else sorted(files["gltf"])[0]]["gltf"]
        get(package["url"], os.path.join(folder, asset + ".gltf"))
        for name, entry in package.get("include", {}).items():
            get(entry["url"], os.path.join(folder, name))
            got.append(os.path.basename(name))
        got += restore_cutouts(asset, files, folder, res)
    print("%s: %s" % (asset, ", ".join(got)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--res", default="2k")
    parser.add_argument("--surface", action="store_true")
    parser.add_argument("--sky", action="store_true")
    parser.add_argument("assets", nargs="+")
    args = parser.parse_args()
    for asset in args.assets:
        fetch(asset, args.res, args.surface, args.sky)


main()
