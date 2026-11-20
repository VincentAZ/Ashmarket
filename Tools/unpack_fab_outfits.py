#!/usr/bin/env python3
"""Unpacks MetaHuman packages (.mhpkg, as Fab's Download button gives them) into the project's Content folder.

A package is a plain zip of Unreal assets with a Manifest.json that says where in /Game they were saved from, and they
find each other by those paths, so each is put back exactly there (Content/Outfits/<Name>/...). Nothing in a package is
run. Content/ is not in the repository: these are licensed to Vincent's account and stay on this machine.

    Tools/unpack_fab_outfits.py ~/Downloads/ashmarket_fab/oa_scavenger.mhpkg [more.mhpkg ...]
    Tools/unpack_fab_outfits.py --list ~/Downloads/ashmarket_fab/*.mhpkg
"""
import json
import os
import sys
import zipfile

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTENT = os.path.join(PROJECT, "Content")


def unpack(path, only_list):
    with zipfile.ZipFile(path) as package:
        names = package.namelist()
        # one thing at the top of the package, or a bundle: several, each in a folder with a manifest of its own
        inner = sorted(name[:-len("Manifest.json")] for name in names if name.endswith("/Manifest.json"))
        for prefix in inner or [""]:
            manifest = json.loads(package.read(prefix + "Manifest.json"))
            asset = manifest["assetData"]["objectPath"].split(".")[0]
            if not asset.startswith("/Game/"):
                raise RuntimeError("%s was not saved under /Game: %s" % (path, asset))
            # the main asset sits at the top of its part of the package, so its own folder is where that part goes
            folder = os.path.dirname(asset)[len("/Game/"):]
            mine = [name for name in names if name.startswith(prefix) and not name.endswith("/") and not name.endswith(".json")]
            wardrobe = [name[len(prefix):-len(".uasset")] for name in mine if os.path.basename(name).startswith("WI_") and "/" not in name[len(prefix):]]
            details = manifest.get("details", {})
            print("%s: %s, %s, made in %s, fits any body: %s, %d files, %.0f MB -> /Game/%s%s" % (
                os.path.basename(path), manifest["name"], manifest.get("assetType"), details.get("engineVersion"),
                details.get("bResizesWithBlendableBodies"), len(mine), manifest.get("totalSize", 0) / 1e6, folder,
                "".join("  wardrobe item /Game/%s/%s" % (folder, name) for name in wardrobe)))
            if only_list:
                continue
            target = os.path.join(CONTENT, folder)
            for name in mine:
                where = os.path.normpath(os.path.join(target, name[len(prefix):]))
                if not where.startswith(target + os.sep) or not name.endswith((".uasset", ".umap", ".uexp", ".ubulk")):
                    raise RuntimeError("%s holds something that is not an asset of its own folder: %s" % (path, name))
                os.makedirs(os.path.dirname(where), exist_ok=True)
                with package.open(name) as source, open(where, "wb") as out:
                    while True:
                        block = source.read(1 << 20)
                        if not block:
                            break
                        out.write(block)


def main():
    arguments = sys.argv[1:]
    only_list = "--list" in arguments
    for path in [argument for argument in arguments if argument != "--list"]:
        unpack(path, only_list)


if __name__ == "__main__":
    main()
