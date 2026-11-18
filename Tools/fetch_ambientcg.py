#!/usr/bin/env python3
"""Downloads photographed decals from ambientCG (ambientcg.com, all CC0) into ArtSource/ambientcg/<id>/.

    Tools/fetch_ambientcg.py [--res 2K] <id> [<id> ...]     for example Leaking003: its colour, opacity, normal and roughness maps

Nothing is made here: the files are stored as ambientCG publishes them, under our names for the maps.
"""
import argparse
import io
import os
import urllib.request
import zipfile

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ArtSource", "ambientcg")
# ambientCG's name for a map -> ours. Its "DX" normal map has green down, as the engine reads it.
MAPS = {"Color": "basecolor", "Opacity": "opacity", "NormalDX": "normal", "Roughness": "roughness"}


def fetch(asset, res):
    name = "{}_{}-JPG".format(asset, res)
    request = urllib.request.Request("https://ambientcg.com/get?file={}.zip".format(name), headers={"User-Agent": "ashmarket-prototype"})
    with urllib.request.urlopen(request, timeout=300) as response:
        archive = zipfile.ZipFile(io.BytesIO(response.read()))
    folder = os.path.join(HERE, asset)
    os.makedirs(folder, exist_ok=True)
    for theirs, ours in MAPS.items():
        member = "{}_{}.jpg".format(name, theirs)
        if member not in archive.namelist():
            print("{}: no {} map".format(asset, ours))
            continue
        with open(os.path.join(folder, "{}_{}.jpg".format(asset, ours)), "wb") as out:
            out.write(archive.read(member))
    print("{}: {}".format(asset, ", ".join(sorted(os.listdir(folder)))))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--res", default="2K")
    parser.add_argument("ids", nargs="+")
    arguments = parser.parse_args()
    for asset in arguments.ids:
        fetch(asset, arguments.res)


main()
