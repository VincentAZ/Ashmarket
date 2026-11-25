#!/usr/bin/env python3
"""Fetches Unreal-format things from the signed-in Epic account's Fab library, with no launcher.

Run with the python that has `legendary` installed, after `legendary auth` has been done once:

    ~/.cache/ashmarket-sim/epic-venv/bin/python Tools/fetch_fab_library.py --list
    ~/.cache/ashmarket-sim/epic-venv/bin/python Tools/fetch_fab_library.py --out DIR "MC Idles" "Sword Animset"

Each title given is matched from its start. Each thing goes into its own folder under DIR, named after
its artifact. The newest engine version the seller lists is taken. `legendary status` refreshes the sign-in.

Only Unreal-format things come this way. MetaHuman packages (.mhpkg) are given out by the website's
Download button alone.
"""
import argparse
import json
import multiprocessing
import os
import sys
import urllib.error
import urllib.request

AGENT = 'UELauncher/17.0.0 Linux'


def signed_in():
    with open(os.path.expanduser('~/.config/legendary/user.json')) as handle:
        user = json.load(handle)
    return user['access_token'], user['account_id']


def ask(url, token, body=None):
    headers = {'Authorization': 'Bearer ' + token, 'User-Agent': AGENT, 'Accept': 'application/json'}
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        headers['Content-Type'] = 'application/json'
    request = urllib.request.Request(url, data=data, headers=headers, method='POST' if data else 'GET')
    with urllib.request.urlopen(request, timeout=120) as reply:
        return json.load(reply)


def library(token, account):
    things, cursor = [], None
    while True:
        url = f'https://www.fab.com/e/accounts/{account}/ue/library?count=100'
        if cursor:
            url += f'&cursor={cursor}'
        page = ask(url, token)
        things += page.get('results', [])
        cursor = (page.get('cursors') or {}).get('next')
        if not cursor:
            return things


def engine_number(version):
    """'UE_5.8' -> (5, 8)."""
    try:
        return tuple(int(part) for part in version.replace('UE_', '').split('.'))
    except ValueError:
        return (0, 0)


def newest(thing):
    return max(thing['projectVersions'], key=lambda v: max((engine_number(e) for e in v.get('engineVersions', [])), default=(0, 0)))


def fetch(thing, token, out):
    from legendary.core import LegendaryCore
    from legendary.downloader.mp.manager import DLManager

    version = newest(thing)
    artifact = version['artifactId']
    where = os.path.join(out, artifact)
    done = os.path.join(where, '.fetched')
    if os.path.exists(done):
        print(f'{thing["title"]}: already in {where}', flush=True)
        return True

    reply = ask(f'https://www.fab.com/e/artifacts/{artifact}/manifest', token,
                {'item_id': thing['assetId'], 'namespace': thing['assetNamespace'], 'platform': 'Windows'})
    info = next(i for i in reply['downloadInfo'] if i.get('type') == 'manifest')
    manifest = None
    for point in info['distributionPoints']:
        try:
            request = urllib.request.Request(point['manifestUrl'], headers={'User-Agent': AGENT})
            with urllib.request.urlopen(request, timeout=120) as got:
                manifest = LegendaryCore.load_manifest(got.read())
            base = point['manifestUrl'].split('?')[0].rsplit('/', 1)[0]
            break
        except (urllib.error.URLError, OSError) as error:
            print(f'  {point["manifestUrl"].split("/")[2]}: {error}', flush=True)
    if manifest is None:
        print(f'{thing["title"]}: no manifest could be read', flush=True)
        return False

    os.makedirs(where, exist_ok=True)
    manager = DLManager(where, base, {}, resume_file=os.path.join(where, '.resume'), case_insensitive=False)
    result = manager.run_analysis(manifest=manifest)
    print(f'{thing["title"]} ({", ".join(version.get("engineVersions", []))}): '
          f'{result.dl_size / 1e9:.2f} GB to fetch, {result.install_size / 1e9:.2f} GB on disk, to {where}', flush=True)
    manager.start()
    manager.join()
    if manager.exitcode != 0:
        print(f'{thing["title"]}: the fetch ended with code {manager.exitcode}', flush=True)
        return False
    with open(done, 'w') as handle:
        handle.write(info.get('buildVersion', '') + '\n')
    print(f'{thing["title"]}: done', flush=True)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--out')
    parser.add_argument('titles', nargs='*')
    args = parser.parse_args()

    token, account = signed_in()
    things = library(token, account)
    if args.list or not args.titles:
        for thing in things:
            version = newest(thing)
            print(f'{thing["title"]} | {thing.get("seller", "")} | {thing.get("distributionMethod", "")} | '
                  f'{version["artifactId"]} | {",".join(version.get("engineVersions", []))}')
        return 0
    if not args.out:
        parser.error('--out is needed to fetch')

    failed = 0
    for title in args.titles:
        match = [t for t in things if t['title'].lower().startswith(title.lower())]
        if len(match) != 1:
            print(f'"{title}": {len(match)} things in the library start that way', flush=True)
            failed += 1
            continue
        if not fetch(match[0], token, args.out):
            failed += 1
    return 1 if failed else 0


if __name__ == '__main__':
    multiprocessing.set_start_method('fork')
    sys.exit(main())
