#!/usr/bin/env python3
"""The compact original (docs/compact.md): a game's files as a disc install leaves them, made small for hosting.

    make_compact.py make --game mw3 --zip "MechWarrior 3 (USA).zip" --mw3 build/mw3 --mw3import build/mw3import
                         --out mw3-compact-1.0.tar.xz --game-min 1.7.0 [--work DIR] [--crf 24]
                         [--by reverse-engineered.com] [--url-base https://reverse-engineered.com/launcher/originals/]

From the disc image's zip (the Internet Archive's, checked by its SHA-256 below): the game's own setup of it
(`mw3 --setup --install`, as the launcher asks), then
  - left out: what the rebuild never reads (the software renderer's texture sets, the Windows DLLs but
    Mech3Msg.dll, the Indeo codec, the installer's leftovers; LEAVE_OUT below, docs/compact.md says why);
  - each movie (video/<name>.avi) as <name>.webm (VP9 of the Indeo pictures mw3import decodes, --frames-out),
    <name>.flac (its sound, lossless) and <name>.fmvh (the .fmv mw3import writes from the AVI);
  - the CD music as music/trackNN.flac;
  - mw3-compact.json first: the format, the game, its version, the source's SHA-256, the encoders, every file's size
    and SHA-256.
The archive is deterministic for the same input and the same encoders: entries sorted, times and owners zero,
single-threaded encoders with bit-exact muxing, Python's lzma. It prints the catalog's "compact" entry.

    make_compact.py check --game mw3 --zip ... --compact mw3-compact-1.0.tar.xz --mw3 build/mw3
                          --mw3import build/mw3import [--work DIR] [--grabs DIR]

The equality check: the disc zip and the compact each installed by the game's own setup (`mw3 --setup --install`,
as the launcher asks), the movies converted by the first start's importer; the two trees compared (the compact's
less the left-out files and the AVIs), every .wav and .fmv byte for byte, the music as decoded sound, every .m1v's
pictures by PSNR and SSIM (frame by frame, by count), and --status on both. Frame grabs side by side into --grabs.
Needs numpy for the pictures.
"""
import argparse
import hashlib
import io
import json
import lzma
import os
import re
import shutil
import subprocess
import sys
import tarfile

SOURCES = {  # the Internet Archive's disc images (the catalog's "original")
    'mw3': {'zip_sha256': '00f365a48c907ccd07b793fb0c3ae7a2e4eed1cbad84c152ae9869d2eb4c2eb5', 'name': 'MechWarrior 3 1.0'},
    'pm': {'zip_sha256': '1b7a6b8f9ffa44470a12c4c4f653d0ab22f4dd52d724cce91609645c20a1935d', 'name': "MechWarrior 3: Pirate's Moon 1.0"},
}
FORMAT = 'compact 1'
MANIFEST = 'mw3-compact.json'

# What the rebuild never reads (lower-case paths relative to the install), docs/compact.md:
LEAVE_OUT = [
    r'ir50_32\.dll',  # the Indeo codec: only the Windows Indeo helper loads it, and only with AVIs
    r'(mfc40|mfc42|msvcirt|msvcp50|msvcp60|msvcrt|msvcrt40|iforce2|uninst)\.dll',  # Mech3.exe's runtime and the uninstaller
    r'msn gaming zone\.url',  # a web shortcut
    r'zbd/[^/]+/texture[0-9]*\.zbd',  # the software renderer's world textures (the port opens only rtexture*)
    r'zbd/rmechtexs\.zbd',  # and its mech textures
    r'mw3-install\.txt',  # the setup's own record (written again by the install)
    r'video/[^/]+\.avi',  # the movies: as .webm, .flac and .fmvh
]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def run(cmd, **kw):
    print('  $', ' '.join(str(c) for c in cmd), flush=True)
    r = subprocess.run([str(c) for c in cmd], **kw)
    if r.returncode != 0:
        sys.exit(f'make_compact: failed ({r.returncode}): {cmd[0]}')
    return r


def disc_install(a, zip_path, work):
    """The game's own setup of the disc image, as the launcher asks it: work/wd/install and work/wd/music."""
    wd = os.path.join(work, 'wd')
    inst, music = os.path.join(wd, 'install'), os.path.join(wd, 'music')
    if os.path.exists(os.path.join(inst, 'mw3-install.txt')):
        return inst, music
    shutil.rmtree(wd, ignore_errors=True)
    os.makedirs(wd)
    r = run([a.mw3, '--setup', '--install', zip_path, '--write-dir', wd, '--nosound', '--hidden'], capture_output=True, text=True)
    last = [json.loads(l) for l in r.stdout.splitlines() if l.startswith('{')]
    if not last or not last[-1].get('ok'):
        sys.exit(f'make_compact: the setup did not install {zip_path}: {last[-1] if last else r.stdout[-400:]}')
    print(f'    {last[-1].get("words", "")} -> {last[-1].get("summary", "")}')
    return inst, music


def encoder_versions(ffmpeg):
    out = subprocess.run([ffmpeg, '-hide_banner', '-version'], capture_output=True, text=True).stdout
    v = {'ffmpeg': out.splitlines()[0] if out else 'unknown'}
    try:
        v['libvpx'] = subprocess.run(['pkg-config', '--modversion', 'vpx'], capture_output=True, text=True).stdout.strip() or 'unknown'
    except OSError:
        v['libvpx'] = 'unknown'
    v['python_lzma'] = f'liblzma via Python {sys.version.split()[0]}'
    return v


def left_out(rel):
    l = rel.lower()
    return any(re.fullmatch(p, l) for p in LEAVE_OUT)


def make(a):
    work = os.path.abspath(a.work)
    os.makedirs(work, exist_ok=True)
    src = SOURCES[a.game]
    print(f'make_compact: {a.zip}', flush=True)
    zip_sha = sha256_file(a.zip)
    if zip_sha != src['zip_sha256'] and not a.any_zip:
        sys.exit(f'make_compact: {a.zip} is not the Internet Archive\'s image ({zip_sha}); --any-zip to take it anyway')
    inst, music = disc_install(a, a.zip, work)
    tree = os.path.join(work, 'compact')
    shutil.rmtree(tree, ignore_errors=True)
    os.makedirs(tree)
    files, dropped = [], []
    for root, _, names in os.walk(inst):
        for n in sorted(names):
            p = os.path.join(root, n)
            rel = os.path.relpath(p, inst).replace(os.sep, '/')
            if left_out(rel):
                if not rel.lower().endswith('.avi') and rel != 'mw3-install.txt':
                    dropped.append(rel)
                continue
            os.makedirs(os.path.dirname(os.path.join(tree, rel)), exist_ok=True)
            shutil.copy2(p, os.path.join(tree, rel))
    # The movies: pictures through VP9, sound through FLAC, the importer's header kept.
    vdir = next((os.path.join(inst, d) for d in os.listdir(inst) if d.lower() == 'video'), None)
    imp = os.path.join(work, 'import')
    if vdir and any(n.lower().endswith('.avi') for n in os.listdir(vdir)):
        if not a.mw3import:
            sys.exit('make_compact: this game has movies: give --mw3import')
        shutil.rmtree(imp, ignore_errors=True)
        run([a.mw3import, '--install', inst, '--write-dir', imp])
        for n in sorted(os.listdir(vdir)):
            if not n.lower().endswith('.avi'):
                continue
            name = n[:-4].lower()
            out = os.path.join(tree, 'video', name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            enc = os.path.join(work, f'enc-crf{a.crf}', name + '.webm')  # kept across runs: the encode is the long part
            if not os.path.exists(enc):
                os.makedirs(os.path.dirname(enc), exist_ok=True)
                y4m = os.path.join(work, name + '.y4m')
                run([a.mw3import, '--install', inst, '--frames-out', y4m, '--only', name])
                run([a.ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', y4m, '-c:v', 'libvpx-vp9', '-crf', str(a.crf),
                     '-b:v', '0', '-row-mt', '0', '-threads', '1', '-g', '150', '-deadline', 'good', '-cpu-used', '1',
                     '-an', '-fflags', '+bitexact', '-flags:v', '+bitexact', '-f', 'webm', enc + '.part'])
                os.replace(enc + '.part', enc)
                os.remove(y4m)
            shutil.copyfile(enc, out + '.webm')
            wav = os.path.join(imp, 'video', name + '.wav')
            if os.path.exists(wav):
                run([a.ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', wav, '-c:a', 'flac', '-sample_fmt', 's16',
                     '-compression_level', '12', '-fflags', '+bitexact', '-flags:a', '+bitexact', out + '.flac'])
            shutil.copyfile(os.path.join(imp, 'video', name + '.fmv'), out + '.fmvh')
    # The music.
    if os.path.isdir(music):
        for n in sorted(os.listdir(music)):
            if re.fullmatch(r'track\d\d\.wav', n.lower()):
                os.makedirs(os.path.join(tree, 'music'), exist_ok=True)
                run([a.ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', os.path.join(music, n), '-c:a', 'flac',
                     '-compression_level', '12', '-fflags', '+bitexact', '-flags:a', '+bitexact',
                     os.path.join(tree, 'music', n[:-4].lower() + '.flac')])
    for root, _, names in os.walk(tree):
        for n in names:
            p = os.path.join(root, n)
            files.append({'path': os.path.relpath(p, tree).replace(os.sep, '/'), 'size': os.path.getsize(p), 'sha256': sha256_file(p)})
    files.sort(key=lambda f: f['path'])
    version = '1.0'
    manifest = {
        'format': FORMAT, 'game': a.game, 'version': version, 'name': src['name'], 'by': a.by,
        'source': {'name': os.path.basename(a.zip), 'sha256': zip_sha},
        'movies': {'video': f'VP9 (libvpx-vp9 -crf {a.crf} -b:v 0, one thread, -g 150) of the Indeo 5 pictures mw3import decodes',
                   'sound': 'FLAC of the AVI\'s PCM, the .wav byte for byte after the import'},
        'encoders': encoder_versions(a.ffmpeg),
        'left_out': sorted(dropped),
        'files': files,
    }
    text = (json.dumps(manifest, indent=1, ensure_ascii=False) + '\n').encode('utf-8')
    # The archive: the manifest first, then every file in path order; nothing of this machine in it.
    def info(name, size, mode=0o644):
        t = tarfile.TarInfo(name)
        t.size, t.mode, t.mtime, t.uid, t.gid, t.uname, t.gname = size, mode, 0, 0, 0, '', ''
        return t
    tmp = a.out + '.part'
    with lzma.open(tmp, 'wb', format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC64, preset=9 | lzma.PRESET_EXTREME) as xz:
        with tarfile.open(fileobj=xz, mode='w', format=tarfile.PAX_FORMAT) as t:
            t.addfile(info(MANIFEST, len(text)), io.BytesIO(text))
            for f in files:
                with open(os.path.join(tree, f['path']), 'rb') as fh:
                    t.addfile(info(f['path'], f['size']), fh)
    os.replace(tmp, a.out)
    size, sha = os.path.getsize(a.out), sha256_file(a.out)
    name = os.path.basename(a.out)
    entry = {'url': a.url_base + name, 'sha256': sha, 'size': size, 'name': name,
             'note': f'{a.by}\'s compact copy of the original game files', 'game_min': a.game_min}
    print(f'make_compact: {a.out}: {len(files)} files, {size / 1e6:.1f} MB (left out: {len(dropped)}, '
          f'the movies {sum(f["size"] for f in files if f["path"].startswith("video/")) / 1e6:.1f} MB)')
    print('catalog "compact" entry:')
    print(json.dumps(entry, indent=2))


def setup_install(a, pick, wd):
    """The game's own setup, as the launcher asks it: the copy into wd/install, its music into the media folder."""
    shutil.rmtree(wd, ignore_errors=True)
    os.makedirs(wd)
    r = run([a.mw3, '--setup', '--install', pick, '--write-dir', wd, '--nosound', '--hidden'], capture_output=True, text=True)
    last = [json.loads(l) for l in r.stdout.splitlines() if l.startswith('{')]
    if not last or not last[-1].get('ok'):
        sys.exit(f'make_compact: the setup did not install {pick}: {last[-1] if last else r.stdout[-400:]}')
    print(f'    {last[-1].get("words", "")} -> {last[-1].get("summary", "")}')
    inst = os.path.join(wd, 'install')
    if any(f.lower().endswith(('.avi', '.webm')) for f in tree(inst)):  # Pirate's Moon has no movies
        run([a.mw3import, '--install', inst, '--write-dir', wd])
    raw = json.loads(subprocess.run([a.mw3, '--status', '--install', inst, '--write-dir', wd, '--nosound', '--hidden'],
                                    capture_output=True, text=True).stdout.strip().splitlines()[-1])
    files = raw.get('files', {})
    return inst, {'state': files.get('state'), 'data': files.get('data'), 'pending': raw.get('first_run', {}).get('pending', [])}


def tree(root):
    out = {}
    for r, _, names in os.walk(root):
        for n in names:
            p = os.path.join(r, n)
            out[os.path.relpath(p, root).replace(os.sep, '/')] = p
    return out


def y4m(ffmpeg, src, dst):
    run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', src, '-fps_mode', 'passthrough', '-f', 'yuv4mpegpipe',
         '-pix_fmt', 'yuv420p', dst])


def pictures(ffmpeg, a, b):
    """PSNR (mean, min) and SSIM (all) of two y4m files, frame for frame by count."""
    r = subprocess.run([ffmpeg, '-hide_banner', '-i', a, '-i', b, '-lavfi', '[0]split[a1][a2];[1]split[b1][b2];[a1][b1]ssim;[a2][b2]psnr',
                        '-f', 'null', '-'], capture_output=True, text=True).stderr
    ssim = re.search(r'All:([0-9.]+)', r)
    psnr = re.search(r'average:([0-9.inf]+) min:([0-9.inf]+)', r)
    return (float(psnr.group(1)), float(psnr.group(2)), float(ssim.group(1))) if ssim and psnr else (0.0, 0.0, 0.0)


def pcm_sha(ffmpeg, path):
    r = subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-i', path, '-f', 's16le', '-acodec', 'pcm_s16le', '-'],
                       capture_output=True)
    return hashlib.sha256(r.stdout).hexdigest() if r.returncode == 0 and r.stdout else ''


def check(a):
    work = os.path.abspath(a.work)
    os.makedirs(work, exist_ok=True)
    print('make_compact: the disc, installed by the setup', flush=True)
    d_inst, d_st = setup_install(a, a.zip, os.path.join(work, 'check-disc'))
    print('make_compact: the compact, installed by the setup', flush=True)
    c_inst, c_st = setup_install(a, a.compact, os.path.join(work, 'check-compact'))
    fails = []
    # The install trees: every file the compact keeps is the disc install's, byte for byte; nothing else but its own.
    dt, ct = tree(d_inst), tree(c_inst)
    kept = 0
    for rel, p in sorted(dt.items()):
        if rel == 'mw3-install.txt' or left_out(rel):
            continue
        if rel not in ct:
            fails.append(f'install: {rel} missing from the compact')
        elif sha256_file(p) != sha256_file(ct[rel]):
            fails.append(f'install: {rel} differs')
        else:
            kept += 1
    extra = [r for r in ct if r not in dt and r != MANIFEST and not re.fullmatch(r'video/[^/]+\.(webm|flac|fmvh)', r.lower())]
    fails += [f'install: {r} is only in the compact' for r in extra]
    print(f'  install: {kept} files identical, {len(fails)} problems')
    # The imported movies and the music, in each media folder (the write dir here).
    dwd, cwd = os.path.join(work, 'check-disc'), os.path.join(work, 'check-compact')
    dv, cv = tree(os.path.join(dwd, 'video')), tree(os.path.join(cwd, 'video'))
    same_bytes, rows = 0, []
    for rel in sorted(dv):
        if rel not in cv:
            fails.append(f'video: {rel} not made from the compact')
            continue
        if rel.endswith(('.wav', '.fmv')):
            if sha256_file(dv[rel]) == sha256_file(cv[rel]):
                same_bytes += 1
            else:
                fails.append(f'video: {rel} differs')
        elif rel.endswith('.m1v'):
            y1, y2 = os.path.join(work, 'd.y4m'), os.path.join(work, 'c.y4m')
            y4m(a.ffmpeg, dv[rel], y1)
            y4m(a.ffmpeg, cv[rel], y2)
            rows.append((rel[:-4],) + pictures(a.ffmpeg, y1, y2))
            if a.grabs:
                os.makedirs(a.grabs, exist_ok=True)
                n = int(subprocess.run(['ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v:0', '-show_entries',
                                        'stream=nb_read_frames', '-of', 'csv=p=0', y1], capture_output=True, text=True).stdout.strip() or 0)
                for k in (n // 5, n // 2, 4 * n // 5):
                    run([a.ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', y1, '-i', y2, '-filter_complex',
                         f"[0]select=eq(n\\,{k})[l];[1]select=eq(n\\,{k})[r];[l][r]hstack", '-frames:v', '1',
                         os.path.join(a.grabs, f'{rel[:-4]}-{k:04d}-disc-left-compact-right.png')])
            os.remove(y1)
            os.remove(y2)
    fails += [f'video: {r} only from the compact' for r in cv if r not in dv]
    print(f'  movies: {same_bytes} .wav/.fmv identical; pictures (PSNR mean / min dB, SSIM), disc import against compact import:')
    for name, pm, pmin, s in rows:
        print(f'    {name:10s} {pm:6.2f} {pmin:6.2f} {s:.5f}')
        if pmin < a.min_psnr:
            fails.append(f'video: {name}.m1v PSNR min {pmin:.2f} below {a.min_psnr}')
    if rows:
        print(f'    all        {sum(r[1] for r in rows) / len(rows):6.2f} {min(r[2] for r in rows):6.2f} {min(r[3] for r in rows):.5f} (mean, worst, worst)')
    dm, cm = tree(os.path.join(dwd, 'music')), tree(os.path.join(cwd, 'music'))
    tracks = 0
    for rel in sorted(dm):
        stem = rel.rsplit('.', 1)[0].lower()
        twin = next((cm[r] for r in cm if r.rsplit('.', 1)[0].lower() == stem), None)
        if not twin:
            fails.append(f'music: {rel} missing from the compact')
        elif pcm_sha(a.ffmpeg, dm[rel]) != pcm_sha(a.ffmpeg, twin):
            fails.append(f'music: {rel} sounds different')
        else:
            tracks += 1
    print(f'  music: {tracks} of {len(dm)} tracks the same sound')
    for st, who in ((d_st, 'disc'), (c_st, 'compact')):
        print(f'  --status ({who}): {st.get("state")} data {st.get("data")} pending {st.get("pending", [])}')
    if d_st.get('state') != 'ready' or c_st.get('state') != 'ready' or d_st.get('data') != c_st.get('data'):
        fails.append(f'status: disc {d_st.get("state")}/{d_st.get("data")}, compact {c_st.get("state")}/{c_st.get("data")}')
    if c_st.get('pending'):
        fails.append(f'status: the compact still has {c_st.get("pending")} pending after the import')
    print(f'make_compact: check {"passed" if not fails else "FAILED"}: {len(fails)} failed')
    for f in fails:
        print('  ' + f)
    sys.exit(1 if fails else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    m = sub.add_parser('make')
    c = sub.add_parser('check')
    for p in (m, c):
        p.add_argument('--game', choices=sorted(SOURCES), required=True)
        p.add_argument('--zip', required=True)
        p.add_argument('--mw3', required=True, help='the game program (mw3, or mw3pm for Pirate\'s Moon)')
        p.add_argument('--mw3import', help='the importer (MechWarrior 3; Pirate\'s Moon has no movies)')
        p.add_argument('--ffmpeg', default='ffmpeg', help='an FFmpeg with libvpx-vp9 (ours on the players\' side only decodes)')
        p.add_argument('--work', default='compact-work')
    m.add_argument('--out', required=True)
    m.add_argument('--crf', type=int, default=20)
    m.add_argument('--by', default='reverse-engineered.com')
    m.add_argument('--url-base', default='https://reverse-engineered.com/launcher/originals/')
    m.add_argument('--game-min', required=True,
                   help="the catalog entry's game_min: the game's own first release that reads compacts "
                        "(MechWarrior 3 1.7.0, Pirate's Moon 1.3.0)")
    m.add_argument('--any-zip', action='store_true', help='take a zip that is not the Internet Archive\'s')
    c.add_argument('--compact', required=True)
    c.add_argument('--grabs', help='a folder for frame grabs, disc import left, compact import right')
    c.add_argument('--min-psnr', type=float, default=38.0, help='the worst frame allowed, dB')
    a = ap.parse_args()
    (make if a.cmd == 'make' else check)(a)


if __name__ == '__main__':
    main()
