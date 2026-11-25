"""
Finds recordings on freesound.org that are published under CC0 1.0 and fetches freesound's own high-quality MP3 preview
of each, which needs no account, into ArtSource/sound/<category>/, writing each into ArtSource/sound/manifest.json with
its page, author and licence as read from the page. (The original files need a free freesound account; each page is kept.)

    python3 -I Tools/fetch_freesound_previews.py --search "distant gunshot" [--longest 30] [--pages 2]
        lists what there is: id, seconds, name, the start of its description. Nothing is fetched.
    python3 -I Tools/fetch_freesound_previews.py --fetch 384717=far/shot_distant_pistol_1 385812=far/shot_single_distant_pistol
        fetches those ids as those names (category/name, with _preview.mp3 put on the end).

Only the search words and sound ids are sent. Needs ffprobe on the path.
"""

import datetime
import html
import json
import pathlib
import re
import subprocess
import sys
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOUND = ROOT / "ArtSource/sound"
AGENT = {"User-Agent": "Mozilla/5.0 (X11; Linux x86_64)"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=60) as reply:
        return reply.read()


def search(words, longest, pages):
    for page in range(1, pages + 1):
        query = urllib.parse.urlencode({"q": words, "f": 'license:"Creative Commons 0" duration:[0.3 TO {}]'.format(longest), "page": page})
        text = get("https://freesound.org/search/?" + query).decode("utf-8", "replace")

        for block in re.split(r'(?=<div class="bw-search__result")', text)[1:]:
            link = re.search(r'href="(/people/[^"]+/sounds/(\d+)/)" title="([^"]*)"', block)
            long = re.search(r'data-duration="([\d.]+)"', block)
            about = re.search(r'<div class="[^"]*bw-search__result-description[^"]*"[^>]*>(.*?)</div>', block, re.S)

            if not link:
                continue

            said = re.sub(r"<[^>]+>", " ", about.group(1)) if about else ""

            print("{:>8} {:>6}s  {}  |  {}".format(link.group(2), long.group(1)[:5] if long else "?", html.unescape(link.group(3))[:60], " ".join(html.unescape(said).split())[:150]))


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=sample_rate,channels,bit_rate:format=duration", "-of", "json", str(path)], capture_output=True, text=True, check=True)
    facts = json.loads(out.stdout)

    return facts["streams"][0], float(facts["format"]["duration"])


def fetch(pairs):
    manifest_path = SOUND / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    known = {entry["file"] for entry in manifest}
    today = datetime.date.today().isoformat()

    for pair in pairs:
        number, name = pair.split("=", 1)
        category = name.split("/")[0]
        file = name + "_preview.mp3"

        if file in known:
            print("have", file)
            continue

        api = get("https://freesound.org/apiv2/sounds/{}/?format=json".format(number)) if False else None
        page = None

        # the page is found by its id: freesound sends the id's own address on to the page with the author in it
        with urllib.request.urlopen(urllib.request.Request("https://freesound.org/s/{}/".format(number), headers=AGENT), timeout=60) as reply:
            page_url = reply.geturl()
            page = reply.read().decode("utf-8", "replace")

        if "creativecommons.org/publicdomain/zero/1.0" not in page:
            print("NOT CC0, left alone:", number, page_url)
            continue

        preview = re.search(r'(https://cdn\.freesound\.org/previews/\d+/{}_\d+-hq\.mp3)'.format(number), page)

        if not preview:
            low = re.search(r'(https://cdn\.freesound\.org/previews/\d+/{}_\d+)-lq\.mp3'.format(number), page)
            preview_url = low.group(1) + "-hq.mp3" if low else None
        else:
            preview_url = preview.group(1)

        if not preview_url:
            print("no preview found for", number)
            continue

        title = re.search(r"<title>(.*?)</title>", page, re.S)
        about = re.search(r'<meta name="description" content="([^"]*)"', page)
        author = re.search(r"/people/([^/]+)/sounds/", page_url)
        kind = re.search(r'<div class="text-grey">\s*Type\s*</div>\s*<div[^>]*>\s*([^<]+)<', page)

        target = SOUND / file
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(get(preview_url))

        stream, seconds = probe(target)

        manifest.append({
            "file": file,
            "category": category,
            "description": "{}: {}".format(html.unescape(title.group(1)).split(" by ")[0].strip() if title else name, html.unescape(about.group(1)) if about else "")[:600],
            "source_site": "freesound.org",
            "page_url": page_url,
            "download_url": preview_url,
            "licence": "CC0 1.0",
            "author": author.group(1) if author else "",
            "is_preview": True,
            "format": "mp3",
            "sample_rate": int(stream.get("sample_rate", 0)),
            "channels": int(stream.get("channels", 0)),
            "seconds": round(seconds, 3),
            "bytes": target.stat().st_size,
            "is_original_file": False,
            "derivation": "freesound's own high-quality MP3 preview of this sound, as served on its page. Licence read from the sound's page on {} (CC0 1.0). Not edited after download.".format(today),
            "freesound_id": int(number),
            "original_download_url_needs_free_account": page_url + "download/",
        })

        print("fetched {} ({:.1f} s) from {}".format(file, seconds, page_url))

    manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n")


def main():
    args = sys.argv[1:]

    if args and args[0] == "--search":
        words = args[1]
        longest = 30
        pages = 1

        if "--longest" in args:
            longest = float(args[args.index("--longest") + 1])

        if "--pages" in args:
            pages = int(args[args.index("--pages") + 1])

        search(words, longest, pages)
    elif args and args[0] == "--fetch":
        fetch(args[1:])
    else:
        print(__doc__)


main()
